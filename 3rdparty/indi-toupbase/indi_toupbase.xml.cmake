<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
        <device label="ToupCam" mdpd="true">
                <driver name="ToupCam">indi_toupcam_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="Altair" mdpd="true">
                <driver name="Altair">indi_altair_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
        <device label="StartshootG" mdpd="true">
                <driver name="StartshootG">indi_starshootg_ccd</driver>
                <version>@TOUPBASE_VERSION_MAJOR@.@TOUPBASE_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>
