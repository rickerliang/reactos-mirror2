<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../tools/rbuild/project.dtd">
<group>
<module name="inflib" type="staticlibrary">
	<include base="inflib">.</include>
	<define name="__NO_CTYPE_INLINES" />
	<pch>inflib.h</pch>
	<file>infcore.c</file>
	<file>infget.c</file>
	<file>infput.c</file>
	<file>infrosgen.c</file>
	<file>infrosget.c</file>
	<file>infrosput.c</file>
</module>
<module name="inflibhost" type="hoststaticlibrary" allowwarnings="true">
	<include base="inflibhost">.</include>
	<include base="ReactOS">include/reactos</include>
	<define name="__NO_CTYPE_INLINES" />
	<compilerflag>-Wpointer-arith</compilerflag>
	<compilerflag>-Wconversion</compilerflag>
	<compilerflag>-Wstrict-prototypes</compilerflag>
	<compilerflag>-Wmissing-prototypes</compilerflag>
	<define name="INFLIB_HOST" />
	<include base="ReactOS">include</include>
	<pch>inflib.h</pch>
	<file>infcore.c</file>
	<file>infget.c</file>
	<file>infput.c</file>
	<file>infhostgen.c</file>
	<file>infhostget.c</file>
	<file>infhostput.c</file>
</module>
</group>
