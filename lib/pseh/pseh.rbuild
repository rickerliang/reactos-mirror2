<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../tools/rbuild/project.dtd">
<module name="pseh" type="staticlibrary">
	<if property="ARCH" value="i386">
		<directory name="i386">
			<file>framebased.S</file>
		</directory>
	</if>
	<if property="ARCH" value="powerpc">
		<directory name="powerpc">
			<file>framebased.S</file>
		</directory>
	</if>
	<file>framebased.c</file>
</module>
