<?xml version="1.0"?>
<!DOCTYPE group SYSTEM "../../tools/rbuild/project.dtd">
<group xmlns:xi="http://www.w3.org/2001/XInclude">
	<module name="hal" type="kernelmodedll" entrypoint="HalInitSystem@8" installbase="system32" installname="hal.dll">
		<importlibrary base="hal" definition="../hal.pspec" />
		<bootstrap installbase="$(CDOUTPUT)/system32" />
		<include>include</include>
		<include base="ntoskrnl">include</include>
		<define name="_NTHALDLL_" />
		<define name="_NTHAL_" />
		<library>hal_generic</library>
		<library>hal_generic_pcat</library>
		<library>hal_generic_up</library>
		<library>ntoskrnl</library>
		<library>libcntpr</library>
		<directory name="up">
			<file>halinit_up.c</file>
			<file>halup.rc</file>
		</directory>
	</module>
</group>
