plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.example.guitarpilot"

    compileSdk {
        version = release(37)
    }

    defaultConfig {
        applicationId = "com.example.guitarpilot2"
        minSdk = 26
        targetSdk = 37
        // Incrementar versionCode en cada APK distribuido como actualización.
        // Conservar applicationId y la misma clave de firma para no perder datos.
        versionCode = 2
        versionName = "1.1"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                arguments("-DANDROID_STL=c++_shared")
            }
        }
	
	ndk {
    abiFilters += listOf("arm64-v8a")
}
    }

    buildTypes {
        release {
            optimization {
                enable = false
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    buildFeatures {
        prefab = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
}

dependencies {
    implementation(libs.androidx.activity.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)

    implementation("com.google.oboe:oboe:1.11.0")

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}