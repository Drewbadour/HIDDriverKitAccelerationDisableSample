//
//  SimpleDriverLoadingView.swift
//  DeliberateDriverLoader
//
// See the LICENSE.txt file for this sample’s licensing information.
//
// Abstract:
// A simple view for managing driver installation and uninstallation.
//

import SwiftUI
import OSLog
import SystemExtensions

struct SimpleDriverLoadingView: View {

	@StateObject var model: SimpleDriverLoaderModel = SimpleDriverLoaderModel()

	var body: some View {
		VStack {
			Text("Deliberate Driver Loader")
				.font(.title)
				.padding()
			HStack {
				Button(
					action: {
						model.activateDext()
					}, label: {
						Text("Install/Update Driver")
					}
				)
				Button(
					action: {
						model.deactivateDext()
					}, label: {
						Text("Uninstall Driver")
					}
				)
			}
		}
		.padding()
	}
}

struct SimpleDriverLoadingView_Previews: PreviewProvider {

	static var model: SimpleDriverLoaderModel = .init()

	static var previews: some View {
		SimpleDriverLoadingView().environmentObject(model)
	}
}
