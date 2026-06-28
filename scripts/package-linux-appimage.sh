#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/release}"
OUTPUT_DIR="${2:-package}"
APP_NAME="DesktopMCPInspector"
APPDIR="${OUTPUT_DIR}/${APP_NAME}.AppDir"
TOOLS_DIR="${OUTPUT_DIR}/tools"

mkdir -p "${OUTPUT_DIR}" "${TOOLS_DIR}"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/share/applications" "${APPDIR}/usr/share/icons/hicolor/scalable/apps"

EXECUTABLE="${BUILD_DIR}/${APP_NAME}"
if [[ ! -x "${EXECUTABLE}" ]]; then
  echo "Expected executable was not found: ${EXECUTABLE}" >&2
  echo "Build the desktop_mcp_inspector target before running this script." >&2
  exit 1
fi

cp "${EXECUTABLE}" "${APPDIR}/usr/bin/${APP_NAME}"

cat > "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Desktop MCP Inspector
Comment=Inspect and audit Model Context Protocol servers
Exec=DesktopMCPInspector
Icon=DesktopMCPInspector
Categories=Development;Utility;
Terminal=false
DESKTOP

cat > "${APPDIR}/usr/share/icons/hicolor/scalable/apps/${APP_NAME}.svg" <<'SVG'
<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <rect x="10" y="10" width="108" height="108" rx="22" fill="#20242c"/>
  <path d="M36 43h56M36 64h56M36 85h36" stroke="#7dd3fc" stroke-width="10" stroke-linecap="round"/>
</svg>
SVG

cp "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" "${APPDIR}/${APP_NAME}.desktop"
cp "${APPDIR}/usr/share/icons/hicolor/scalable/apps/${APP_NAME}.svg" "${APPDIR}/${APP_NAME}.svg"

LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_PLUGIN="${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [[ ! -x "${LINUXDEPLOY}" ]]; then
  curl -L --fail --retry 3 -o "${LINUXDEPLOY}" \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  chmod +x "${LINUXDEPLOY}"
fi

if [[ ! -x "${LINUXDEPLOY_QT_PLUGIN}" ]]; then
  curl -L --fail --retry 3 -o "${LINUXDEPLOY_QT_PLUGIN}" \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
  chmod +x "${LINUXDEPLOY_QT_PLUGIN}"
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export OUTPUT="${APP_NAME}.AppImage"
export QMAKE="${QMAKE:-qmake6}"

(
  cd "${OUTPUT_DIR}"
  "tools/linuxdeploy-x86_64.AppImage" \
    --appdir "${APP_NAME}.AppDir" \
    --plugin qt \
    --output appimage
)
