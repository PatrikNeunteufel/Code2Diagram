// Musterprojekt fuer Code2Diagram: eine kleine Notiz-App in der heutigen Form (Compose, Coroutinen).
// Die Bau-Skripte (.kts) liest Code2Diagram noch nicht (Entscheid K6).
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "Notizen"
include(":app")
