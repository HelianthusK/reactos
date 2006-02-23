<?xml version="1.0"?>
<rbuild xmlns:xi="http://www.w3.org/2001/XInclude">
  <module name="imagesoft" type="win32gui" installbase="system32" installname="imagesoft.exe">
    <include base="imagesoft">.</include>
    <define name="UNICODE" />
    <define name="_UNICODE" />
    <define name="__REACTOS__" />
    <define name="__USE_W32API" />
    <define name="_WIN32_IE">0x600</define>
    <define name="_WIN32_WINNT">0x501</define>
    <library>kernel32</library>
    <library>gdi32</library>
    <library>user32</library>
    <library>advapi32</library>
    <library>version</library>
    <library>comctl32</library>
    <library>shell32</library>
    <library>comdlg32</library>
    <compilationunit name="unit.c">
      <file>about.c</file>
      <file>floattoolbar.c</file>
      <file>imagesoft.c</file>
      <file>opensave.c</file>
    </compilationunit>
    <file>imagesoft.rc</file>
    <pch>imagesoft.h</pch>
  </module>
</rbuild>
