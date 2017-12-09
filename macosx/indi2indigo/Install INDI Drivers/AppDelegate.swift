//
//  AppDelegate.swift
//  Install INDI Drivers
//
//  Created by Peter Polakovic on 28/07/2017.
//  Copyright © 2017 CloudMakers, s. r. o. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate, NSTableViewDataSource, XMLParserDelegate {
  
  @IBOutlet weak var window: NSWindow!
  @IBOutlet weak var driversTableView: NSTableView!
  
  let BUNDLE = Bundle.main
  let FILE_MANAGER = FileManager.default
  let RESOURCE_URL = Bundle.main.resourceURL
  let GROUP_URL = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "AG8BXW65A2.INDIGO")!
  
  var drivers: [String] = [ ]
  var labels: [String] = [ ]
  var statuses: [Bool] = [ ]
  
  func numberOfRows(in tableView: NSTableView) -> Int {
    return drivers.count
  }
  
  func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
    if tableColumn?.identifier == "Install" {
      return statuses[row] ? NSOnState : NSOffState
    } else {
      return labels[row]
    }
  }
  
  func tableView(_ tableView: NSTableView, setObjectValue object: Any?, for tableColumn: NSTableColumn?, row: Int) {
    if let value = object as? NSNumber {
      statuses[row] = value.boolValue
    }
  }
  
  func parser(_ parser: XMLParser, didStartElement elementName: String, namespaceURI: String?, qualifiedName qName: String?, attributes attributeDict: [String : String] = [:]) {
    if elementName == "driver" {
      if let name = attributeDict["name"], let description = attributeDict["description"], let version = attributeDict["version"] {
        if FILE_MANAGER.fileExists(atPath: URL(string: name, relativeTo: RESOURCE_URL)!.path) {
          drivers.append(name)
          labels.append("\(description) (\(version))")
          statuses.append(FILE_MANAGER.fileExists(atPath: URL(string: "Drivers/\(name)", relativeTo: GROUP_URL)!.path))
          driversTableView.reloadData()
        } else {
          NSLog("Driver \(name) skipped")
        }
      }
    }
  }
  
  @IBAction func showSourceCode(_ sender: AnyObject) {
    if let url = URL(string: "https://github.com/indilib/indi") {
      NSWorkspace.shared().open(url)
    }
  }
  
  @IBAction func install(_ sender: AnyObject) {
    let alert = NSAlert()
    do {
      for i in 0..<drivers.count {
        let name = drivers[i]
        let target = URL(string: "Drivers/\(name)", relativeTo: GROUP_URL)!
        if statuses[i] {
          let source = URL(string: name, relativeTo: RESOURCE_URL)!
          try FILE_MANAGER.copyItem(at: source, to: target)
        } else if FILE_MANAGER.fileExists(atPath: target.path) {
          try FILE_MANAGER.removeItem(at: target)
        }
      }
      try FILE_MANAGER.copyItem(at: URL(string: "indi.xml", relativeTo: RESOURCE_URL)!, to: URL(string: "Drivers/indi.xml", relativeTo: GROUP_URL)!)
      alert.messageText = "Done"
      alert.informativeText = "Drivers are successfully installed/removed."
      alert.alertStyle = .informational
      alert.addButton(withTitle: "Close")
      alert.beginSheetModal(for: window)
    } catch let error as NSError {
      alert.messageText = "Failed"
      alert.informativeText = error.localizedDescription
      alert.alertStyle = .critical
      alert.addButton(withTitle: "Close")
      alert.beginSheetModal(for: window)
    }
  }
  
  func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
    return true
  }
  
  func applicationDidFinishLaunching(_ notification: Notification) {
    do {
      try FILE_MANAGER.createDirectory(at: URL(string: "Drivers", relativeTo: GROUP_URL)!, withIntermediateDirectories: true, attributes: nil)
    } catch let error as NSError {
      let alert = NSAlert()
      alert.messageText = "Failed"
      alert.informativeText = error.localizedDescription
      alert.alertStyle = .critical
      alert.addButton(withTitle: "Close")
      alert.beginSheetModal(for: window)
    }
    if let driversFile = BUNDLE.url(forResource: "indi", withExtension: "xml") {
      if let parser = XMLParser(contentsOf: driversFile) {
        parser.delegate = self
        parser.shouldProcessNamespaces = false
        parser.shouldReportNamespacePrefixes = false
        parser.shouldResolveExternalEntities = false
        parser.parse()
      }
    }
  }
  
  func applicationWillTerminate(_ notification: Notification) {
  }
}

