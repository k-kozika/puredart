plugins {
    id("com.android.library")
}

val puredartRoot = rootProject.file("../..").canonicalPath
val puredartMeta = providers.gradleProperty("puredartMeta").orNull
    ?: error("Pass -PpuredartMeta=/absolute/path/to/blutter_meta.cpp")

android {
    namespace = "com.example.puredartdemo"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        minSdk = 23
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DPUREDART_ROOT=$puredartRoot",
                    "-DPUREDART_META_FILE=$puredartMeta",
                    "-DANDROID_STL=c++_static",
                )
                abiFilters += "arm64-v8a"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
