# INDI Drivers

This directory includes all the built-in INDI drivers arrange by driver **family** type.

# Driver Documentation

All drivers must provide complete and up to date user Documentation. The documentation is hosted on INDI official site in the [devices](https://indilib.org/devices.html) page.

# Documentation Template

Below are some documentation templates that can be used to write new documentation for your driver. Be direct, simple, and make no assumptions on the user knowledge. The documentation should be friendly to new useres with clear instructions accompined with screenshots and visual aids to ensure proper communication.

Screenshots for the driver functionalities are required. You can use whatever GUI client of your choice when submitting the screenshots. The format must be JPG and named after the INDI group/tab they belong to (e.g. main_control.jpg, options.jpg..etc).

Driver metadata is essential and includes the driver name, executable, version, author, and manufacturer.

When submitting a driver to be merged in INDI, please submit along with it documentation that includes:

+ Documentation template filled for your driver.
+ Screenshot + Images used in the documentation (jpg only).
+ Driver Metadata:
++ Website
++ Driver Name: Default driver name (**not** label).
++ Driver Executable: indi_drivername_family..etc
++ Family: Telescopes, Focusers, CCDs..etc
++ Manufacturer
++ Platforms: Which OS does it run on? The usual for INDI drivers is: Linux, BSD, OSX.
++ Author
++ Version

## CCD

```
<h2><i class="fa fa-star" data-mce-empty="1"> </i> Installation</h2>
<p>
INDI DRIVER_NAME Driver currently supports XXXXXXXXX.
1. Link to supported hardware?
2. Any exceptions?
3. Type of devices? Just cameras? or filter wheels? what else?
</p>
<p>Under Ubuntu, you can install the driver via:</p>
<pre>
sudo add-apt-repository ppa:mutlaqja/ppa
sudo apt-get update
sudo apt-get install indi-XXX
</pre>
<h2><i class="fa fa-plus"></i> Features</h2>
<p style="text-align: center;"><img src="images/devices/XXXX/main_control.jpg" alt="Main Control Panel" /></p>
<p>
The driver supports capture, binning, setting temperature, gain and offset adjustment....
1. Can it guide?
2. Supports internal filter wheel?
3. Color or mono? RGB? Bayer?
4. Single or multiple image formats?
5. Video streaming?
6. Temperature control?
7. Fan control?
</p>
<h2><i class="fa fa-gears" data-mce-empty="1"> </i> Operation</h2>
<h3>Connecting to DRIVER_NAME</h3>
<p>
1. How is the device connectd? USB? Ethernet? WiFi..etc?
2. Dedicate a section to each connection method. Put images of each. What configuration required..etc
3. Any special settings to take care of to ensure successful connection? e.g. turn off "Auto Sleep" mode in DSLR camera.
</p>
<h3>General Info</h3>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/general_info.jpg" alt="General Info" /></p>
This provides general information about the currently running driver and driver version.  It also lets you set the Observer and Object Information for the FITS Header.
<h3>Capture</h3>
<p>
Talk about how capture is done. Does it support subframing? different color formats? special controls for gain, offset..etc? Bayer support?
</p>
<h3>Options</h3>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/options.jpg" alt="Options" /></p>
<p>
The Options tab contains settings for default file locations, upload behavior, and debugging. The polling period for this driver should be kept as is unless you need to reduce it for a specific reason.

<h4>Saving locally</h4>
You can opt to save all captured images to the local storage. This is the local storage of the device running the INDI GPhoto driver and not the SD Card. Settings for saving to SD card is also available. Under Options tab you can select the Upload mode:

Client: Upload image to client (default).
Local: Store image locally, never send to client.
Both: Store image locally and send it to client as well.

<h4>Driver Specific Options</h4>
Add here any driver-specific options in the tab.

</p>
<h3>Image Settings</h3>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/image_settings.jpg" alt="Image Settings" /></p>

<h3>Image Info</h3>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/image_info.jpg" alt="Image Info" /></p>
<p>
The image info tab contains information on the resolution of the CCD (Maximum Width & Height) in addition to the pixel size in microns. If the camera supports Bayer mask, then the bayer filter and offset can be set here. These are usually set automatically by the driver, but can be adjusted manually if needed.
</p>


<h2><i class="fa fa-bugs" data-mce-empty="1"> </i> Issues</h2>
There are no known bugs for this driver. If you found a bug, please report it at INDI's <a href="https://sourceforge.net/p/indi/bugs/">bug tracking system</a> at SourceForge. (You can log in with a variety of existing accounts, including Google, Yahoo and OpenID.)
<form action="http://www.indilib.org/download.html">
	<p style="text-align:center;">
		<button class="btn btn-large btn-primary" type="submit">Download Now!</button>
	</p>
</form>
```

## Focusers

```
<h2><i class="fa fa-star" data-mce-empty="1"> </i> Installation</h2>
<p>INDI DRIVER_NAME driver is included with libindi >= INDI_VERSION Under Ubuntu, you can install the driver via:</p>
<pre>sudo add-apt-repository ppa:mutlaqja/ppa</pre>
<pre>sudo apt-get update</pre>
<pre>sudo apt-get install DRIVER_NAME</pre>

<h2><i class="fa fa-plus"></i> Features</h2>

<p style="text-align: center;"><img src="images/devices/XXXX/main_control.jpg" alt="Main Control Panel" /></p>
<p>
List what the driver supports.
1. What kind of focusing? Open loop or closed loop?
2. Absolute or relative focuser?
3. Adjustable speeds?
4. Temperature compensation?
5. Backlash compensation?
6. Sync support?
7. Reverse motion?
8. ...etc
</p>

<h2><i class="fa fa-gears" data-mce-empty="1"></i> Operation</h2>

<h3>Connecting to DRIVER_NAME</h3>
<p>
1. How is the device connectd? USB? Ethernet? WiFi..etc?
2. Dedicate a section to each connection method. Put images of each. What configuration required..etc
3. Any special settings to take care of to ensure successful connection?
</p>
<p>
After connecting, how can you operate the focuser?
</p>
<h2><i class="fa fa-sliders-h" data-mce-empty="1"></i>Options</h2>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/options.jpg" alt="Options" /></p>
<p>
The Options tab contains settings for all drivers that include polling (frequency of updates), logging, and debugging. Additional driver-specific options can be added here. If you have any additional settings, list them in detail.

<h2><i class="fa fa-grip-horizontal" data-mce-empty="1"> </i>Presets</h2>
<p>You may set pre-defined presets for common focuser positions in the <i>Presets</i> tab.</p>
<ul>
	<li>Preset Positions: You may set up to 3 preset positions. When you make a change, the new values will be saved in the driver's configuration file and are loaded automatically in subsequent uses.</li>
	<li>Preset GOTO: Click any preset to go to that position</li>
</ul>
<h2><i class="fa fa-bug"></i> Issues</h2>
<p>There are no known bugs for this driver. If you found a bug, please report it at INDI's <a href="https://github.com/indilib/indi/issues">bug tracking system</a> at Github.</p>
<form action="http://www.indilib.org/download.html">
	<p style="text-align: center;">
		<button type="submit" class="btn btn-large btn-primary">Download Now!</button>
	</p>
</form>
<p><span style="font-size: 1.5em;"></span></p>
```

