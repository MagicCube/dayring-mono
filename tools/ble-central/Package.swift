// swift-tools-version: 5.9
import PackageDescription
import Foundation

let packageRoot = URL(fileURLWithPath: #filePath).deletingLastPathComponent().path
let package = Package(
    name: "DayringBLE",
    platforms: [.macOS(.v13), .iOS(.v16)],
    products: [
        .library(name: "DayringBLE", targets: ["DayringBLE"]),
        .executable(name: "dayring-ble", targets: ["DayringCLI"]),
    ],
    targets: [
        .target(name: "DayringBLE"),
        .executableTarget(name: "DayringCLI", dependencies: ["DayringBLE"], linkerSettings: [
            .unsafeFlags(["-Xlinker", "-sectcreate", "-Xlinker", "__TEXT", "-Xlinker", "__info_plist",
                          "-Xlinker", packageRoot + "/Info.plist"], .when(platforms: [.macOS])),
        ]),
        .testTarget(name: "DayringBLETests", dependencies: ["DayringBLE"]),
    ]
)
