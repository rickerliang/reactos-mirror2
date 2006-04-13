<module name="setupapi" type="win32dll" baseaddress="${BASEADDRESS_SETUPAPI}" installbase="system32" installname="setupapi.dll" allowwarnings="true">
	<importlibrary definition="setupapi.spec.def" />
	<include base="setupapi">.</include>
	<include base="ReactOS">include/reactos/wine</include>
	<include base="pnp_client">.</include>
	<define name="UNICODE" />
	<define name="_UNICODE" />
	<define name="__REACTOS__" />
	<define name="__USE_W32API" />
	<define name="_WIN32_IE">0x600</define>
	<define name="_WIN32_WINNT">0x501</define>
	<define name="WINVER">0x501</define>
	<define name="__WINESRC__" />
	<define name="_SETUPAPI_" />
	<define name="_SETUPAPI_VER">0x501</define>
	<library>pnp_client</library>
	<library>uuid</library>
	<library>wine</library>
	<library>ntdll</library>
	<library>kernel32</library>
	<library>advapi32</library>
	<library>user32</library>
	<library>rpcrt4</library>
	<library>version</library>
	<file>cfgmgr.c</file>
	<file>devinst.c</file>
	<file>dirid.c</file>
	<file>diskspace.c</file>
	<file>install.c</file>
	<file>misc.c</file>
	<file>parser.c</file>
	<file>queue.c</file>
	<file>setupcab.c</file>
	<file>stringtable.c</file>
	<file>stubs.c</file>
	<file>rpc.c</file>
	<file>setupapi.rc</file>
	<file>setupapi.spec</file>
</module>
