plugins {
    base
}

tasks.register<Sync>("stageRelease") {
    dependsOn(":demo:assembleRelease")
    into(layout.buildDirectory.dir("payload"))
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE

    from({ zipTree(project(":demo").layout.buildDirectory.file("outputs/aar/demo-release.aar").get().asFile) }) {
        include("jni/arm64-v8a/libpuredart_demo.so")
        eachFile { path = name }
        includeEmptyDirs = false
    }
}
