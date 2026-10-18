plugins {
    id("com.android.application")
    kotlin("android")
}

android {
    namespace = "de.beispiel.wetter"
    compileSdk = 34
    defaultConfig {
        applicationId = "de.beispiel.wetter"
        minSdk = 24
        targetSdk = 34
    }
    buildFeatures {
        viewBinding = false
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
}
