<?xml version="1.0" encoding="UTF-8"?>
<driversList>
<devGroup group="CCDs">
        <device label="Atik CCD" mdpd="true">
                <driver name="Atik">indi_atik_ccd</driver>
                <version>@ATIK_VERSION_MAJOR@.@ATIK_VERSION_MINOR@</version>
	</device>
</devGroup>
<devGroup group="Filter Wheels">
        <device label="Atik EFW" mdpd="true">
                <driver name="Atik">indi_atik_wheel</driver>
                <version>@ATIK_VERSION_MAJOR@.@ATIK_VERSION_MINOR@</version>
        </device>
</devGroup>
</driversList>
