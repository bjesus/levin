plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.yoavmoshe.levin"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.yoavmoshe.levin"
        minSdk = 26
        targetSdk = 34
        versionCode = 3
        versionName = "0.0.3"

        ndk {
            // armeabi-v7a dropped: OpenSSL ARM assembly has PIC relocation
            // issues when linked into shared libs, and 32-bit ARM devices
            // are a negligible fraction of active Android installs.
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DLEVIN_USE_STUB_SESSION=OFF",
                    "-DLEVIN_BUILD_TESTS=OFF",
                    "-DANDROID_STL=c++_shared",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
                )
            }
        }
    }

    signingConfigs {
        // Use the shared debug key for release builds.
        // Good enough for a sideloaded open-source app.
        named("debug") {
            // Uses default debug.keystore automatically
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    kotlinOptions {
        jvmTarget = "1.8"
    }

    ndkVersion = "27.0.12077973"

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1+"
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.preference:preference-ktx:1.2.1")
    implementation("androidx.lifecycle:lifecycle-service:2.7.0")
}
