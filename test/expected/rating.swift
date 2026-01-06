//
// RatingGenerated.swift
// Generated from rating.riv
//
// ⚠️ DO NOT EDIT - This file is auto-generated from rating.riv
// Regenerate with: make rive-codegen
//
// MARK: - Rive File Structure
//
// ┌─ Default Configuration
// │  • Artboard: Complete
// │  • State Machine: 
// │  • View Model: 
// │
// ├─ Artboards
// │  ├─ [DEFAULT] Complete
// │  │  ├─ Animation: Thumbnail
// │  │  ├─ Animation: 5_stars
// │  │  ├─ Animation: 4_stars
// │  │  ├─ Animation: 3_stars
// │  │  ├─ Animation: 2_stars
// │  │  ├─ Animation: 1_star
// │  │  ├─ Animation: Idle_empty
// │  │  ├─ State Machine: State Machine 1
// │
// └─ View Models
//

import DependencyInjection
import RiveRuntime
import SwiftUI
import UIKit
import WDSKit
import WNStorage

// MARK: - Rating ViewModel

/// View model for rating.riv (no data binding)
@MainActor
public final class RiveRatingViewModel: WDSRiveViewModel {
    // MARK: - MetaData

    public struct MetaData {

        public static let fileName = "rating"

    }
// MARK: - Initializer

    public init(
        autoPlay: Bool = false,
        loadCdn: Bool = false,
        assetLoader: RiveAssetLoader? = FontOnlyAssetLoader(),
        fit: WDSRiveFit = .contain,
        alignment: WDSRiveAlignment = .center,
        bundle: Bundle? = nil
    ) {
        super.init(
            configuration: .init(
                fileName: "rating",
                autoPlay: autoPlay,
                loadCdn: loadCdn,
                assetLoader: assetLoader,
                fit: fit,
                alignment: alignment,
                bundle: bundle ?? .module
            )
        )
    }
}
