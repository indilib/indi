<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
        <device label="ZWO CCD" mdpd="true">
                <driver name="ZWO CCD">indi_asi_ccd</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
	</device>
</devGroup>
<devGroup group="Filter Wheels">
        <device label="ASI Wheel" mdpd="true">
                <driver name="ASI Wheel">indi_asi_wheel</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>

