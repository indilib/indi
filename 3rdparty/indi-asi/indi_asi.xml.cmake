<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
        <device label="ZWO CCD" mdpd="true">
                <driver name="ZWO CCD">indi_asi_ccd</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
	</device>
</devGroup>
<devGroup group="Filter Wheels">
        <device label="ASI EFW" mdpd="true">
                <driver name="ASI EFW">indi_asi_wheel</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Focusers">
        <device label="ASI EAF">
                <driver name="ASI EAF">indi_asi_focuser</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
</devGroup>
<devGroup group="Auxiliary">
        <device label="ASI ST4">
                <driver name="ASI ST4">indi_asi_st4</driver>
                <version>@ASI_VERSION_MAJOR@.@ASI_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>
