<module name="ddraw" type="win32dll" installbase="system32" installname="ddraw.dll">
  <importlibrary definition="ddraw.def" />
	<include base="ddraw">.</include>	
	<define name="UNICODE" />
	<define name="__USE_W32API" />
	<define name="WINVER">0x0600</define>
	<define name="_WIN32_WINNT">0x0501</define>
           
	<library>ntdll</library>
	<library>kernel32</library>
	<library>gdi32</library>
	<library>d3d8thk</library>
	<library>dxguid</library>
	<library>ole32</library>
      <library>user32</library>

	<file>ddraw.rc</file>
	<file>main.c</file>
	<file>regsvr.c</file>
	<file>startup.c</file>
	<file>cleanup.c</file>

      <file>clipper.c</file>
	<file>color.c</file>
	<file>gamma.c</file>
	<file>palette.c</file>
	<file>videoport.c</file>
	<file>kernel.c</file>

	<directory name="hal">		
		<file>surface_hal.c</file>
	</directory>

	<directory name="main">
		<file>ddraw_main.c</file>
		<file>surface_main.c</file>				
	</directory>

	<directory name="soft">		
		<file>surface_hel.c</file>
            <file>surface_callbacks_hel.c</file>
	</directory>

	<directory name="thunks">
		<file>ddraw_thunk.c</file>
		<file>surface_thunk.c</file>
	</directory>
</module>
