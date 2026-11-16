; Azravibe Recorder — Windows NSIS installer
;   scripts/package-windows.nsi
; Expects AZRAVIBE_DIR (path to staged install tree from windeployqt + FFmpeg DLLs)

!define APP_NAME "Azravibe Recorder"
!define APP_EXE "azravibe-recorder.exe"
!define APP_VERSION "1.3.0"
!define APP_PUBLISHER "11nawid"

Name "${APP_NAME}"
OutFile "azravibe-recorder-${APP_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\Azravibe Recorder"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

; Modern UI
!include "MUI2.nsh"
!define MUI_ICON "${AZRAVIBE_DIR}\azravibe-recorder.ico"
!define MUI_UNICON "${AZRAVIBE_DIR}\azravibe-recorder.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${AZRAVIBE_DIR}\LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!define MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Section "Install"
    SetOutPath "$INSTDIR"
    ; The staged tree produced by windeployqt + FFmpeg DLL copy
    File /r "${AZRAVIBE_DIR}\*.*"

    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\azravibe-recorder.ico"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    CreateShortCut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\azravibe-recorder.ico"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Azravibe Recorder" \
        "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Azravibe Recorder" \
        "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Azravibe Recorder" \
        "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Azravibe Recorder" \
        "Publisher" "${APP_PUBLISHER}"

    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    RMDir /r "$INSTDIR"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Azravibe Recorder"
SectionEnd
