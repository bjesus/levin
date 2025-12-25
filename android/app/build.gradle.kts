plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.yoavmoshe.levin"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.yoavmoshe.levin"
        minSdk = 24
        targetSdk = 34
        versionCode = 6
        versionName = "0.3.1"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    
    kotlinOptions {
        jvmTarget = "1.8"
    }
    
    buildFeatures {
        viewBinding = true
        buildConfig = true
    }
}

dependencies {
    // Core Android
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    
    // Lifecycle & Coroutines
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-service:2.7.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")
    
    // Preferences
    implementation("androidx.preference:preference-ktx:1.2.1")
    
    // Navigation
    implementation("androidx.navigation:navigation-fragment-ktx:2.7.6")
    implementation("androidx.navigation:navigation-ui-ktx:2.7.6")
    
    // Fragment
    implementation("androidx.fragment:fragment-ktx:1.6.2")
    
    // libtorrent4j
    val libtorrentVersion = "2.1.0-38"
    implementation("org.libtorrent4j:libtorrent4j:$libtorrentVersion")
    implementation("org.libtorrent4j:libtorrent4j-android-arm:$libtorrentVersion")
    implementation("org.libtorrent4j:libtorrent4j-android-arm64:$libtorrentVersion")
    implementation("org.libtorrent4j:libtorrent4j-android-x86:$libtorrentVersion")
    implementation("org.libtorrent4j:libtorrent4j-android-x86_64:$libtorrentVersion")
    
    // HTTP client for Anna's Archive integration
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    
    // Testing
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}
