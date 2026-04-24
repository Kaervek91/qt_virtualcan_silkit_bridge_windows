set "SILKIT_BIN_PATH=C:\Program Files\Vector SIL Kit 5.0.4"

pushd %SILKIT_BIN_PATH%

start ./sil-kit-registry.exe -u silkit://localhost:8500
start ./sil-kit-system-controller.exe --connect-uri silkit://localhost:8500 QtBridgeParticipant

popd
