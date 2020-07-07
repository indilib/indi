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

## Mount

```
<h2><i class="fa fa-star"></i> Installation</h2>
<p>INDI DRIVER_NAME Driver is released as a 3rd party driver and requires INDI Library &gt;= INDI_VERSION. To Install the driver under Ubuntu:</p>
<pre>sudo apt-add-repository ppa:mutlaqja/ppa
sudo apt-get update &amp;&amp; sudo apt-get install indi-DRIVER_NAME
</pre>
<h2><i class="fa fa-check-circle-o"></i> Features</h2>
<p>INDI DRIVER_NAME driver interacts with a mount controller using the XXXX protocol through a serial link.</p>
<p>Mention who makes the mounts, what models are supported exactly.</p>
<p><img style="display: block; margin-left: auto; margin-right: auto;" src="/images/devices/DRIVER_NAME/main_control.jpg" alt="Main Control" /></p>
<p>Current features are:</p>
<ul>
	<li>GOTO? Sync?</li>
	<li>Slew Speeds?</li>
	<li>Track Rates? Track Modes?</li>
	<li>Can you control tracking? On? Off?</p>
	<li>Meridian Flip?</li>
	<li>Configurable custom parking positions?</li>
	<li>Pulse-guiding?</li>
	<li>Guide Rates?</li>
	<li>ST4 Guiding?</li>
</ul>
<p>Does the driver use the INDI Alignment subsystem to build a model for the sky? or it has its own internal model.</p>
<p>Does it need to be aligned before it can be used with the driver? What is the default home position?</p>
<h2>Connectivity</h2>
<h3>1. USB</h3>
<p>Mention here how this mounts connects via USB. What kind of cables are required. Any special settings needed? baud rate? ..etc.</p>
<h3>2. Network</h3>
<p>Does this driver supports networked connections over TCP/IP? If yes how? Is it wired only or wireless or both?</p>
<p>What are the IP address and port settings? what steps are necessary to connect to this mount step by step?</p>
<h3>3. Bluetooth</h3>
<p>Can you connect to the mount via Bluetotth? which adapter are required? Include a visual guide as well on how to connect via Bluetooth.</p>
<h3>4. First Time Connection</h3>
<p><img style="display: block; margin-left: auto; margin-right: auto;" src="/images/devices/DRIVER_NAME/connection.jpg" alt="Connection" /></p>
<p>Before starting the driver, power the mount and make sure it is looking at the celestial pole with the weights down.</p>
<p>When running the driver for the first time, go to the <b>Connection</b> tab and select the port to connect to. You can also try connecting directly and the driver shall automatically scan the system for candidate ports. If DRIVER_NAME is connected the network via a Serial-TCP (Ethernet or WiFi) adapter, then you can select <em>Ethernet</em> mode and enter the IP address and port for the adapter connected to the mount. After making changes in the Connections tab, go to <strong>Options</strong> tab and save the settings.</p>
<h2><i class="fa fa-gears"></i> Operation</h2>
<h3>Main Control</h3>
<p>The main control tab is where the primary control of DRIVER_NAME takes place. To track an object, enter the equatorial of date (JNow) coordinates and press Set. The mount shall then slew to an object and once it arrives at the target location, it should engage tracking at the selected tracking rate which default to Sidereal tracking. Slew mode is different from track mode in that it does not engage tracking when slew is complete. To sync, the mount must be already tracking. First change mode to Sync, then enter the desired coordinates then press Set. Users will seldom use this interface directly since many clients (e.g. KStars) can slew and sync the mount directly from the sky map without having to enter any coordinates manually.</p>
<p>XXX tracking modes are supported: Sidereal, Lunar, Solar, and Custom. When using <em>Custom</em> mode, the rates defined in <strong>Track Rates</strong> shall be used. Tracking can be enganged and disenganged by toggling the <strong>Tracking</strong> property.</p>
<h3>Options</h3>
<p>&nbsp;Under the options tab, you can configure many parameters before and after you connect to the mount.</p>
<ul>
	<li><strong>Snoop Devices</strong>: Indicate which devices DRIVER_NAME should communicate with:
		<ul>
			<li><strong>GPS</strong>: If using a GPS driver (e.g. INDI GPSD) then enter its name here. DRIVER_NAME shall sync its time and location settings from the GPS driver.</li>
			<li><strong>Dome</strong>: If using a Dome driver, put its name here so that Dome Parking Policy can be applied.</li>
		</ul>
	</li>
	<li><strong>Configuration</strong>: Load or Save the driver settings to a file. Click default to restore default settings that were shipped with the driver.</li>
	<li><strong>Simulation</strong>: Enable to disable simulation mode for testing purposes.</li>
	<li><strong>Debug</strong>: Enable debug logging where verbose messaged can be logged either directly in the client or a file. If Debug is enabled, advanced properties are created to select how to direct debug output. <a href="/support/logs-submission.html">Watch a video on how to submit logs</a>.</li>
	<li>
		<h4 style="font-size: 13px;">Dome Parking Policy</h4>
		<p>If a dome is used in conjunction with the mount, a policy can be set if parking the mount or dome can interfere with the safety of either. For example, you might want to always park the mount&nbsp;<em>before</em>&nbsp;parking the dome, or vice versa. The default policy is to ignore the dome.</p>
		<ul>
			<li><strong>Ignore dome</strong>: Take no action when dome parks or unparks.</li>
			<li><strong>Dome locks</strong>:&nbsp;<strong>Prevent</strong>&nbsp;the mount from unparking when dome is parked.</li>
			<li><strong>Dome parks</strong>:&nbsp;Park the mount if dome starts parking. This will disable the locking for dome parking,&nbsp;<span style="color: #800000;">EVEN IF MOUNT PARKING FAILS</span>.</li>
			<li><strong>Both</strong>: Dome locks &amp; Dome parks policies are applied.</li>
		</ul>
		<br /><img style="display: block; margin-left: auto; margin-right: auto;" src="/images/devices/DRIVER_NAME/options.jpg" alt="Options" /></li>
	<li><strong>Scope Properties</strong>: Enter the Primary and Seconday scope information.&nbsp;Up to six different configurations for&nbsp;<em>Primary</em>&nbsp;and Secondary&nbsp;<em>Guider</em>&nbsp;telescopes can be saved separately, each with an optional unique label in <strong>Scope Name</strong> property.</li>
	<li><strong>Scope Config</strong>: Select the active scope configuration.</li>
	<li><strong>Joystick</strong>: Enable or Disable joystick support. An INDI Joystick driver must be running for this function to work. For more details, check the <a href="/support/tutorials/135-controlling-your-telescope-with-a-joystick.html">INDI Telescope Joystick</a> tutorial.</li>	
</ul>
<h3>Motion Control</h3>
<p><img style="display: block; margin-left: auto; margin-right: auto;" src="/images/devices/DRIVER_NAME/motion_control.jpg" alt="Motion Control" /></p>
<p>Under motion control, manual motion controls along with speed and guide controls are configured.</p>
<ul>
	<li><strong>Motion N/S/W/E</strong>: Directional manual motion control. Press the button to start the movement and release the button to stop.</li>
	<li><strong>Slew Rate</strong>: Rate of manual motion control above where 1x equals one sidereal rate.</li>
	<li><strong>Guide N/S/W/E</strong>: Guiding pulses durations in milliseconds. This property is meant for guider application (e.g. PHD2) and not intended to be used directly.</li>
	<li><strong>Guiding Rate</strong>: Guiding Rate for RA &amp; DE. 0.3 means the mount shall move at 30% of the sidereal rate when the pulse is active. The sideral rate is ~15.04 arcseconds per second. So at 0.3x, the mount shall move 0.3*15.04 = 4.5 arcsecond per second. When receving a pulse for 1000ms, the total theoritical motion 4.5 arcseconds.</li>
	<li><strong>Custom Speeds</strong>: Customs speeds in RA &amp; DEC axis when performing&nbsp;GOTO.</li>
	<li><strong>Track Default</strong>: Default tracking rate to be used on startup.</li>
	<li><strong>ST4 N/S/W/E</strong>: If the mount is receiving guiding pulses via ST4, apply this rate.</li>
</ul>
<p class="alert alert-info">The <b>Slew Rate</b> dropdown is used to control the <b>manual</b> speeds when using the NSWE controls either directly or via a joystick. To set the <b>GOTO</b> speeds (when mount moves from one target to another via a GOTO command), you need to update the <b>Custom Speeds</b> control.</p>
<h3>Site Management</h3>
<p>Time, Locaiton, and Park settings are configured in the Site Management tab.</p>
<p><img style="display: block; margin-right: auto; margin-left: auto;" src="/images/devices/DRIVER_NAME/site_management.jpg" alt="Site Management" /></p>
<ul>
	<li><strong>UTC</strong>: UTC time and offsets must be set for proper operation of the driver upon connection. The UTC offset is in hours. East is positive and west is negative.</li>
	<li><strong>Location</strong>: Latitude and Longitude must be set for proper operation of the driver upon connection. The longitude range is 0 to 360 degrees increasing eastward from Greenwich.</li>
	<li><strong>Parking Position</strong>: Upon connection to the mount, Ekos loads these values into the mount's motor controller to initialize the (stepper) motor step values. The default values represent the home position where the mount points to the celestial pole - i.e. 0 deg RA, 90 deg DEC.</li>
	<li><strong>Parking</strong>:&nbsp;To set the parking position of the mount to the home position, click “Default”, then "Write Data " - this saves the home values as the parking values. To set the parking position of the mount to a custom position, slew the mount to the desired position and click “Current”, then "Write Data " - this saves the current motor step values as the parking values.
		<ul>
			<li><strong>IMPORTANT</strong>: For the first time Ekos connects to the mount, or if for any reason the parking position has become incorrect. Make sure the mount is in the home position, power up the mount, connect Ekos and set the parking position to home by clicking “Default”, then "Write Data ".</li>
		</ul>
	</li>
</ul>
<h2><i class="fa fa-bug "></i> Issues</h2>
<p>There are no known bugs for this driver. If you find a bug, please report it at INDI's <a href="https://github.com/indilib/indi/issues ">Github issues</a> page</p>
<form action="http://www.indilib.org/download.html ">
	<p style="text-align: center;">
		<button class="btn btn-large btn-primary " type="submit ">Download Now!</button>
	</p>
</form>
```
## CCD

```
<h2><i class="fa fa-star" data-mce-empty="1"></i> Installation</h2>
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
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/main_control.jpg" alt="Main Control Panel" /></p>
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
<p>To capture a single-frame image, simple set the desired exposure time in seconds and click <b>Set</b>. After the capture is complete, it should be downloaded as a FITS image. If the camera is equipped with a cooler, target temperature can be set. To change the format and bit depth (if supported), select a different image format in the <b>Controls</b> tab.</p>
<h2><i class="fa fa-gears" data-mce-empty="1"></i> Operation</h2>
<h3>Connecting to DRIVER_NAME</h3>
<p>
1. How is the device connected? USB? Ethernet? WiFi..etc?
2. Dedicate a section to each connection method. Put images of each. What configuration required..etc
3. Any special settings to take care of to ensure successful connection? e.g. turn off "Auto Sleep" mode in DSLR camera.
</p>
<h3>General Info</h3>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/general_info.jpg" alt="General Info" /></p>
This provides general information about the currently running driver and driver version.  It also lets you set the Observer and Object Information for the FITS Header.
<h3>Options</h3>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/options.jpg" alt="Options" /></p>
<p>The Options tab contains settings for default file locations, upload behavior, and debugging. The polling period for this driver should be kept as is unless you need to reduce it for a specific reason.</p>
<ol>
	<li><strong>Debug</strong>: Toggle driver debug logging on/off</li>
	<li><strong>Configuration</strong>: After changing driver settings, click <b>Save</b> to save the changes to the configuration file. The saved values should be used when starting the driver again in the future. The configuration file is saved to the user home directory under <b>.indi</b> directory in an XML file.(e.g. ~/.indi/camera_name.xml)</li>
	<li><strong>Snoop Device</strong>: The camera driver can <i>listen</i> to properties defined in other drivers. This can be used to store the relevant information in the FITS header (like the mount's RA and DE coordinates). The respective drivers (Telescope, Focuser..etc) are usually set by the client, but can be set directly if desired.</li>
	<li><strong>Rapid Guide</strong>: Rapid Guide uses internal algorithm to automataically select guide stars.</li>
	<li><strong>Telescope</strong>: Toggle between <i>Primary</i> and <i>Guide</i> scope selection. This selection is required in order to calculate World-Coordinate-System (WCS) values like Field-Of-View (FOV). When <b>WCS</b> is enabled, the FITS header is populated with WCS keywords that enable clients to map the sources in the image to physical coordinates in the sky. Usually, you do not need to toggle this setting manually as it is usually set by the client automatically</li>
	<li><strong>Upload</strong>: Selects how the captured image is saved/uploaded?
		<ul>
			<li><strong>Client</strong>: The image is uploaded the client (i.e. Ekos or SkyCharts)</li>
			<li><strong>Local</strong>: The image is saved to local storage only.</li>
			<li><strong>Both</strong>: The image is saved to local storage and also uploaded to the client.</li>
		</ul>
	</li>
	<li><strong>Upload Settings</strong>: Sets the <i>local</i> desired directory and prefix used to save the image when either the <b>Local</b> or <b>Both</b> upload modes are active. The <i>IMAGE_XXX</i> is automatically replaced by the image name where the XXX is the image counter (i.e. M42_005.fits). The driver scan the local storage and increments the counter automatically when a new image is captured and stored.</li>
</ol>
<h3>Image Settings</h3>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/image_settings.jpg" alt="Image Settings" /></p>
<p>
  In the <i>Image Settings</i> tab, you can configure the framing and binning of the captured image:
</p>
<ul>
  <li><strong>Frame</strong>: Set the desired <i>Region-Of-Interest</i> (ROI) by specifying the starting X and Y positions of the image and the desired width and height. It is recommended to set use even numbers only to enable binning if required. The ROI values are indenepdent of the binning used.</li>
<li><strong>Binning</strong>: Set the desired binning.</li>
<li>The usually supported image compression can be turned on in image settings to compress FITS images. This might require more processing but can reduce the size of the image by <b>up to 70%</b>. The uploaded image would have an extenstion of .fits.fz and it can be viewed in multiple clients like KStars.</li>
<li>The <b>Frame Type</b> property is used to mark the frame type in the FITS header which is useful information for some processing applications. If there an electronic or mechanical shutter, the driver closes it automatically when taking dark frames.</li>
<li>To restore the ROI to the default values, click on the <b>Reset</b> button.</li>
<h3>Image Info</h3>
<p style="text-align: center;"><img src="images/devices/DRIVER_NAME/image_info.jpg" alt="Image Info" /></p>
<p>The image info tab contains information on the resolution of the CCD (Maximum Width & Height) in addition to the pixel size in microns. If the camera supports Bayer mask, then the bayer filter and offset can be set here. These are usually set automatically by the driver, but can be adjusted manually if needed.</p>
<h2><i class="fa fa-bugs"></i> Issues</h2>
There are no known bugs for this driver. If you found a bug, please report it at INDI's <a href="https://github.com/indilib/indi/issues">Issue Tracking System</a> at Github.
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
<h2><i class="fa fa-bugs"></i> Issues</h2>
There are no known bugs for this driver. If you found a bug, please report it at INDI's <a href="https://github.com/indilib/indi/issues">Issue Tracking System</a> at Github.
<form action="http://www.indilib.org/download.html">
	<p style="text-align:center;">
		<button class="btn btn-large btn-primary" type="submit">Download Now!</button>
	</p>
</form>
```

