<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../../tools/rbuild/project.dtd">
<module name="kbdusx" type="kernelmodedll" entrypoint="0" installbase="system32" installname="kbdusx.dll" allowwarnings="true">
	<importlibrary definition="kbdusx.def" />
	<include base="ntoskrnl">include</include>
	<define name="_DISABLE_TIDENTS" />
	<define name="_WIN32_WINNT">0x0500</define>
	<file>kbdusx.c</file>
	<file>kbdusx.rc</file>
</module>
