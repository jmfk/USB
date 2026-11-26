// swift-tools-version: 5.9
import PackageDescription
let package = Package(name: "USBBench", platforms: [.macOS(.v14)], products: [.executable(name: "USBBench", targets: ["USBBench"])], targets: [
    .target(name: "BenchCore"),
    .executableTarget(name: "USBBench", dependencies: ["BenchCore"]),
    .testTarget(name: "BenchCoreTests", dependencies: ["BenchCore"])
])
