# Add project specific ProGuard rules here.

# libtorrent4j
-keep class org.libtorrent4j.swig.libtorrent_jni {*;}
-keep class org.libtorrent4j.** {*;}

# Keep data classes
-keep class com.yoavmoshe.levin.data.** {*;}

# Keep Kotlin metadata
-keep class kotlin.Metadata { *; }
