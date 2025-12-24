#include "power_monitor.hpp"
#include "logger.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#if defined(__linux__) && defined(HAVE_POWER_MONITOR)
#include <gio/gio.h>
#define POWER_MONITOR_LINUX
#elif defined(__APPLE__) && defined(HAVE_POWER_MONITOR)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#define POWER_MONITOR_MACOS
#endif

namespace levin {

#ifdef POWER_MONITOR_LINUX
// Linux implementation using DBus/UPower
struct PowerMonitor::Impl {
    GDBusConnection* connection = nullptr;
    guint subscription_id = 0;
    std::atomic<bool> on_ac_power{true};
    PowerCallback callback;
    std::atomic<bool> running{false};
    std::thread monitor_thread;
    std::mutex init_mutex;
    std::condition_variable init_cv;
    std::atomic<bool> initial_state_ready{false};
    GMainLoop* main_loop = nullptr;

    static void on_properties_changed(GDBusConnection* /*connection*/,
                                       const gchar* /*sender_name*/,
                                       const gchar* object_path,
                                       const gchar* interface_name,
                                       const gchar* signal_name,
                                       GVariant* parameters,
                                       gpointer user_data) {
        auto* self = static_cast<Impl*>(user_data);
        
        // Only handle UPower DisplayDevice changes
        if (g_strcmp0(object_path, "/org/freedesktop/UPower/devices/DisplayDevice") != 0) {
            return;
        }

        // Parse the properties changed signal
        GVariant* changed_properties;
        g_variant_get(parameters, "(s@a{sv}@as)", nullptr, &changed_properties, nullptr);

        GVariantIter iter;
        g_variant_iter_init(&iter, changed_properties);
        const gchar* property_name;
        GVariant* value;

        while (g_variant_iter_next(&iter, "{&sv}", &property_name, &value)) {
            if (g_strcmp0(property_name, "State") == 0) {
                guint32 state = g_variant_get_uint32(value);
                // UPower states: 1=charging, 2=discharging, 4=fully-charged
                bool new_on_ac = (state == 1 || state == 4);
                
                if (self->on_ac_power != new_on_ac) {
                    self->on_ac_power = new_on_ac;
                    LOG_INFO("Power state changed: {}", new_on_ac ? "AC power" : "Battery");
                    if (self->callback) {
                        self->callback(new_on_ac);
                    }
                }
            }
            g_variant_unref(value);
        }
        g_variant_unref(changed_properties);
    }

    bool query_current_state() {
        GError* error = nullptr;
        GDBusProxy* proxy = g_dbus_proxy_new_sync(
            connection,
            G_DBUS_PROXY_FLAGS_NONE,
            nullptr,
            "org.freedesktop.UPower",
            "/org/freedesktop/UPower/devices/DisplayDevice",
            "org.freedesktop.UPower.Device",
            nullptr,
            &error
        );

        if (error) {
            LOG_WARN("Failed to create UPower proxy: {}", error->message);
            g_error_free(error);
            return true; // Default to AC power
        }

        GVariant* state_variant = g_dbus_proxy_get_cached_property(proxy, "State");
        if (state_variant) {
            guint32 state = g_variant_get_uint32(state_variant);
            bool is_ac = (state == 1 || state == 4); // charging or fully-charged
            g_variant_unref(state_variant);
            g_object_unref(proxy);
            return is_ac;
        }

        g_object_unref(proxy);
        return true; // Default to AC power
    }
};

#elif defined(POWER_MONITOR_MACOS)
// macOS implementation using IOKit
struct PowerMonitor::Impl {
    CFRunLoopRef run_loop = nullptr;
    CFRunLoopSourceRef run_loop_source = nullptr;
    std::atomic<bool> on_ac_power{true};
    PowerCallback callback;
    std::atomic<bool> running{false};
    std::thread monitor_thread;
    std::mutex init_mutex;
    std::condition_variable init_cv;
    std::atomic<bool> initial_state_ready{false};

    static void power_source_callback(void* context) {
        auto* self = static_cast<Impl*>(context);
        bool new_on_ac = self->query_current_state();
        
        if (self->on_ac_power != new_on_ac) {
            self->on_ac_power = new_on_ac;
            LOG_INFO("Power state changed: {}", new_on_ac ? "AC power" : "Battery");
            if (self->callback) {
                self->callback(new_on_ac);
            }
        }
    }

    bool query_current_state() {
        CFTypeRef power_source_info = IOPSCopyPowerSourcesInfo();
        if (!power_source_info) {
            return true; // Default to AC power
        }

        CFArrayRef power_sources = IOPSCopyPowerSourcesList(power_source_info);
        if (!power_sources) {
            CFRelease(power_source_info);
            return true;
        }

        bool is_ac = true;
        CFIndex count = CFArrayGetCount(power_sources);
        
        for (CFIndex i = 0; i < count; i++) {
            CFTypeRef source = CFArrayGetValueAtIndex(power_sources, i);
            CFDictionaryRef description = IOPSGetPowerSourceDescription(power_source_info, source);
            
            if (description) {
                CFStringRef power_source_state = (CFStringRef)CFDictionaryGetValue(
                    description, CFSTR(kIOPSPowerSourceStateKey));
                
                if (power_source_state && 
                    CFStringCompare(power_source_state, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo) {
                    is_ac = false;
                    break;
                }
            }
        }

        CFRelease(power_sources);
        CFRelease(power_source_info);
        return is_ac;
    }
};

#else
// Stub implementation for platforms without power monitoring
struct PowerMonitor::Impl {
    std::atomic<bool> on_ac_power{true};
    PowerCallback callback;
    std::atomic<bool> running{false};
    
    bool query_current_state() {
        return true; // Always assume AC power on unsupported platforms
    }
};
#endif

PowerMonitor::PowerMonitor() : impl_(std::make_unique<Impl>()) {}

PowerMonitor::~PowerMonitor() {
    stop();
}

void PowerMonitor::start(PowerCallback callback) {
    if (impl_->running) {
        LOG_WARN("PowerMonitor already running");
        return;
    }

    impl_->callback = callback;
    impl_->running = true;

#ifdef POWER_MONITOR_LINUX
    impl_->monitor_thread = std::thread([this]() {
        GError* error = nullptr;
        impl_->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
        
        if (error) {
            LOG_ERROR("Failed to connect to system bus: {}", error->message);
            g_error_free(error);
            impl_->running = false;
            {
                std::lock_guard<std::mutex> lock(impl_->init_mutex);
                impl_->initial_state_ready = true;
            }
            impl_->init_cv.notify_one();
            return;
        }

        // Query initial state
        impl_->on_ac_power = impl_->query_current_state();
        LOG_INFO("Initial power state: {}", impl_->on_ac_power ? "AC power" : "Battery");
        
        // Signal that initial state is ready
        {
            std::lock_guard<std::mutex> lock(impl_->init_mutex);
            impl_->initial_state_ready = true;
        }
        impl_->init_cv.notify_one();

        // Subscribe to UPower signals
        impl_->subscription_id = g_dbus_connection_signal_subscribe(
            impl_->connection,
            "org.freedesktop.UPower",
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged",
            "/org/freedesktop/UPower/devices/DisplayDevice",
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            Impl::on_properties_changed,
            impl_.get(),
            nullptr
        );

        // Run GLib main loop
        GMainContext* context = g_main_context_new();
        g_main_context_push_thread_default(context);
        impl_->main_loop = g_main_loop_new(context, FALSE);

        // Run the main loop (this will block until quit is called)
        g_main_loop_run(impl_->main_loop);

        g_main_loop_unref(impl_->main_loop);
        impl_->main_loop = nullptr;
        g_main_context_pop_thread_default(context);
        g_main_context_unref(context);

        if (impl_->subscription_id > 0) {
            g_dbus_connection_signal_unsubscribe(impl_->connection, impl_->subscription_id);
            impl_->subscription_id = 0;
        }

        if (impl_->connection) {
            g_object_unref(impl_->connection);
            impl_->connection = nullptr;
        }
    });

#elif defined(POWER_MONITOR_MACOS)
    impl_->monitor_thread = std::thread([this]() {
        // Query initial state
        impl_->on_ac_power = impl_->query_current_state();
        LOG_INFO("Initial power state: {}", impl_->on_ac_power ? "AC power" : "Battery");
        
        // Signal that initial state is ready
        {
            std::lock_guard<std::mutex> lock(impl_->init_mutex);
            impl_->initial_state_ready = true;
        }
        impl_->init_cv.notify_one();

        // Create run loop source for power notifications
        impl_->run_loop_source = IOPSNotificationCreateRunLoopSource(
            Impl::power_source_callback, impl_.get());
        
        if (!impl_->run_loop_source) {
            LOG_ERROR("Failed to create IOKit run loop source");
            impl_->running = false;
            return;
        }

        impl_->run_loop = CFRunLoopGetCurrent();
        CFRunLoopAddSource(impl_->run_loop, impl_->run_loop_source, kCFRunLoopDefaultMode);

        while (impl_->running) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, true);
        }

        CFRunLoopRemoveSource(impl_->run_loop, impl_->run_loop_source, kCFRunLoopDefaultMode);
        CFRelease(impl_->run_loop_source);
        impl_->run_loop_source = nullptr;
        impl_->run_loop = nullptr;
    });

#else
    // For unsupported platforms, just query once and assume AC power
    impl_->on_ac_power = impl_->query_current_state();
    LOG_INFO("Power monitoring not supported on this platform, assuming AC power");
#endif

#if defined(POWER_MONITOR_LINUX) || defined(POWER_MONITOR_MACOS)
    // Wait for initial state to be determined (with timeout)
    std::unique_lock<std::mutex> lock(impl_->init_mutex);
    impl_->init_cv.wait_for(lock, std::chrono::seconds(2), 
                            [this] { return impl_->initial_state_ready.load(); });
#endif
}

void PowerMonitor::stop() {
    if (!impl_->running) {
        return;
    }

    LOG_INFO("Stopping power monitor");
    impl_->running = false;

#ifdef POWER_MONITOR_LINUX
    if (impl_->main_loop && g_main_loop_is_running(impl_->main_loop)) {
        g_main_loop_quit(impl_->main_loop);
    }
#endif

#if defined(POWER_MONITOR_LINUX) || defined(POWER_MONITOR_MACOS)
    if (impl_->monitor_thread.joinable()) {
        impl_->monitor_thread.join();
    }
#endif
}

bool PowerMonitor::is_on_ac_power() const {
    return impl_->on_ac_power;
}

} // namespace levin
