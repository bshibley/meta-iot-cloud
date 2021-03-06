inherit cmake pkgconfig

DEPENDS = "\
	azure-c-shared-utility \
	azure-uamqp-c \
	azure-umqtt-c \
	azure-iot-sdk-c \
	glib-2.0 \
    libuv \
	nanomsg \
"

# Patches
SRC_URI += "\
    file://Use-provided-dependencies.patch \
    file://Skip-tests-if-requested.patch \
    file://Include-parson-with-main-library.patch \
    file://Fix-iothub-library-name.patch \
    file://Fix-nanomsg-include.patch \
"

# Sample build scripts
SRC_URI += "\
	file://azure-functions-sample.sh \
	file://ble-gateway-sample.sh \
	file://dynamically-add-module-sample.sh \
	file://hello-world-sample.sh \
	file://native-module-host-sample.sh \
	file://proxy-sample.sh \
	file://simulated-device-cloud-upload-sample.sh \
	file://azure-functions-module.sh \
	file://ble-module.sh \
	file://hello-world-module.sh \
	file://identitymap-module.sh \
	file://iothub-module.sh \
	file://logger-module.sh \
	file://simulated-device-module.sh \
"

S = "${WORKDIR}/git/v1"
B = "${WORKDIR}/build"

# Default packages
PACKAGES = "\
	${PN} \
	${PN}-dev \
	${PN}-dbg \
	${PN}-modules \
	${PN}-modules-src \
	${PN}-samples \
	${PN}-samples-src \
	${PN}-dotnetcore \
	${PN}-java \
"

## Java ##
def get_jdk_arch(d):
    jdk_arch = d.getVar('TRANSLATED_TARGET_ARCH', True)

    if jdk_arch == "x86-64":
        jdk_arch = "amd64"
    elif jdk_arch == "powerpc":
        jdk_arch = "ppc"
    elif jdk_arch == "powerpc64":
        jdk_arch = "ppc64"
    elif (jdk_arch == "i486" or jdk_arch == "i586" or jdk_arch == "i686"):
        jdk_arch = "i386"

    return jdk_arch

def get_jdk_home(d):
    jdk_home = d.getVar("STAGING_LIBDIR", True)
    jdk_home += "/jvm/"

    if os.path.exists(jdk_home):
        for child in os.listdir(jdk_home):
            test_path = os.path.join(jdk_home, child)
            if os.path.isdir(test_path):
                jdk_home = test_path
                break

    return jdk_home

## Java ##
JDK_ARCH = "${@get_jdk_arch(d)}"
JDK_HOME = "${@get_jdk_home(d)}"

PACKAGECONFIG ??= "java dotnetcore bluetooth"

PACKAGECONFIG[java] = "-Denable_java_binding:BOOL=ON -Denable_java_remote_modules:BOOL=ON -DJDK_ARCH=${JDK_ARCH}, -Denable_java_binding:BOOL=OFF -Denable_java_remote_modules:BOOL=OFF, openjdk-7"
PACKAGECONFIG[dotnetcore] = "-Denable_dotnet_core_binding:BOOL=ON, -Denable_dotnet_core_binding:BOOL=OFF, dotnet-native"
PACKAGECONFIG[bluetooth] = "-Denable_ble_module:BOOL=ON, -Denable_ble_module:BOOL=OFF, , bluez5"

EXTRA_OECMAKE = "-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS:BOOL=ON -Dinstall_modules:BOOL=ON -Dinstall_executables:BOOL=ON -Drun_as_a_service:BOOL=OFF -Drun_unittests:BOOL=OFF"

do_configure_prepend() {
	# Java
	if ${@bb.utils.contains('PACKAGECONFIG','java','true','false',d)}; then
		export JAVA_HOME="${JDK_HOME}"
	fi

	# .NET Core
	if ${@bb.utils.contains('PACKAGECONFIG','dotnetcore','true','false',d)}; then
		sed -i 's|\${CMAKE_CURRENT_BINARY_DIR}/\.\.|${S}|g' ${S}/CMakeLists.txt		
		sed -i 's|projects_to_test=\"$binding_path/Microsoft.Azure.Devices.Gateway.Tests/Microsoft.Azure.Devices.Gateway.Tests.csproj\"|projects_to_test=\"\"|g' ${S}/tools/build_dotnet_core.sh
	fi
}

do_compile_prepend() {
	# Java
	if ${@bb.utils.contains('PACKAGECONFIG','java','true','false',d)}; then
		export JAVA_HOME="${JDK_HOME}"
	fi

	# .NET Core
	if ${@bb.utils.contains('PACKAGECONFIG','dotnetcore','true','false',d)}; then
		${S}/tools/build_dotnet_core.sh --config Release
	fi
}

do_install_prepend() {
	# Fix sample module paths
	find ${S}/samples -type f -name "*.json" -exec sed -i 's|\.\./\.\./modules|${libdir}/azureiotedge/modules|g' {} +
	find ${S}/samples -type f -name "*.json" -exec sed -i 's|\./modules|${libdir}/azureiotedge/modules|g' {} +
	find ${S}/samples -type f -name "*.json" -exec sed -i 's|build/modules|${libdir}/azureiotedge/modules|g' {} +
	sed -i 's|build/samples/ble_gateway/ble_printer|\.|g' ${S}/samples/ble_gateway/src/*.json
}

do_install() {
    # Core
    install -d ${D}${libdir}
	install -m 0755 ${B}/core/libgateway.so ${D}${libdir}

	install -d ${D}${includedir}/azureiotedge
	install -m 0644 ${S}/core/inc/*.h ${D}${includedir}/azureiotedge
	install -m 0644 ${S}/core/inc/linux/*.h ${D}${includedir}/azureiotedge
	install -m 0644 ${S}/deps/parson/*.h ${D}${includedir}/azureiotedge

	install -d ${D}${includedir}/azureiotedge/experimental
	install -m 0644 ${S}/core/inc/experimental/*.h ${D}${includedir}/azureiotedge/experimental

	install -d ${D}${includedir}/azureiotedge/module_loaders
	install -m 0644 ${S}/core/inc/module_loaders/dynamic_loader.h ${D}${includedir}/azureiotedge/module_loaders
	install -m 0644 ${S}/proxy/outprocess/inc/module_loaders/*.h ${D}${includedir}/azureiotedge/module_loaders
	
	if ${@bb.utils.contains('PACKAGECONFIG','java','true','false',d)}; then
		install -m 0644 ${S}/core/inc/module_loaders/java_loader.h ${D}${includedir}/azureiotedge/module_loaders
	fi
	
	if ${@bb.utils.contains('PACKAGECONFIG','dotnetcore','true','false',d)}; then
		install -m 0644 ${S}/core/inc/module_loaders/dotnet_core_loader.h ${D}${includedir}/azureiotedge/module_loaders
	fi

	# Native Proxy Gateway
	install -d ${D}${libdir}
	install -m 0755 ${B}/proxy/gateway/native/*.so ${D}${libdir}

	install -d ${D}${includedir}/azureiotedge
	install -m 0644 ${S}/proxy/gateway/native/inc/*.h ${D}${includedir}/azureiotedge
	install -m 0644 ${S}/proxy/message/inc/*.h ${D}${includedir}/azureiotedge

	# Native Module Host
	install -d ${D}${libdir}
	install -m 0755 ${B}/proxy/modules/native_module_host/*.so ${D}${libdir}

	install -d ${D}${includedir}/azureiotedge
	install -m 0644 ${S}/proxy/modules/native_module_host/inc/*.h ${D}${includedir}/azureiotedge

	# Modules
	install -d ${D}${includedir}/azureiotedge/modules/common
	install -m 0644 ${S}/modules/common/*.h ${D}${includedir}/azureiotedge/modules/common

	install -d ${D}${exec_prefix}/src/azureiotedge/modules/common
	install -m 0644 ${S}/modules/common/*.h ${D}${exec_prefix}/src/azureiotedge/modules/common/

	# Azure Functions Module
	install -d ${D}${libdir}/azureiotedge/modules/azure_functions
	install -m 0755 ${B}/modules/azure_functions/*.so ${D}${libdir}/azureiotedge/modules/azure_functions/

	install -d ${D}${includedir}/azureiotedge/modules/azure_functions
	install -m 0644 ${S}/modules/azure_functions/inc/*.h ${D}${includedir}/azureiotedge/modules/azure_functions

	install -d ${D}${exec_prefix}/src/azureiotedge/modules/azure_functions/src
	install -d ${D}${exec_prefix}/src/azureiotedge/modules/azure_functions/inc
	install -m 0644 ${S}/modules/azure_functions/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/azure_functions/src/
	install -m 0644 ${S}/modules/azure_functions/inc/*.h ${D}${exec_prefix}/src/azureiotedge/modules/azure_functions/inc/
	install -m 0755 ${WORKDIR}/azure-functions-module.sh ${D}${exec_prefix}/src/azureiotedge/modules/azure_functions/build.sh

	# BLE Module
	if [ -e ${B}/modules/ble/ ]; then
		install -d ${D}${libdir}/azureiotedge/modules/ble
		install -m 0755 ${B}/modules/ble/*.so ${D}${libdir}/azureiotedge/modules/ble/

		install -d ${D}${includedir}/azureiotedge/modules/ble
		install -m 0644 ${S}/modules/ble/inc/*.h ${D}${includedir}/azureiotedge/modules/ble
		install -m 0644 ${S}/modules/ble/deps/linux/dbus-bluez/inc/*.h ${D}${includedir}/azureiotedge/modules/ble

		install -d ${D}${exec_prefix}/src/azureiotedge/modules/ble/src
		install -d ${D}${exec_prefix}/src/azureiotedge/modules/ble/inc
		install -d ${D}${exec_prefix}/src/azureiotedge/modules/ble/deps/dbus-bluez/src
		install -d ${D}${exec_prefix}/src/azureiotedge/modules/ble/deps/dbus-bluez/inc
		install -m 0644 ${S}/modules/ble/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/ble/src/
		install -m 0644 ${S}/modules/ble/inc/*.h ${D}${exec_prefix}/src/azureiotedge/modules/ble/inc/
		install -m 0644 ${S}/modules/ble/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/ble/src/
		install -m 0644 ${S}/modules/ble/deps/linux/dbus-bluez/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/ble/deps/dbus-bluez/src/
		install -m 0644 ${S}/modules/ble/deps/linux/dbus-bluez/inc/*.h ${D}${exec_prefix}/src/azureiotedge/modules/ble/deps/dbus-bluez/inc/
		install -m 0755 ${WORKDIR}/ble-module.sh ${D}${exec_prefix}/src/azureiotedge/modules/ble/build.sh
	fi

	# Hello World Module
	install -d ${D}${libdir}/azureiotedge/modules/hello_world
	install -m 0755 ${B}/modules/hello_world/*.so ${D}${libdir}/azureiotedge/modules/hello_world/

	install -d ${D}${includedir}/azureiotedge/modules/hello_world
	install -m 0644 ${S}/modules/hello_world/inc/*.h ${D}${includedir}/azureiotedge/modules/hello_world

	install -d ${D}${exec_prefix}/src/azureiotedge/modules/hello_world/src
	install -d ${D}${exec_prefix}/src/azureiotedge/modules/hello_world/inc
	install -m 0644 ${S}/modules/hello_world/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/hello_world/src/
	install -m 0644 ${S}/modules/hello_world/inc/*.h ${D}${exec_prefix}/src/azureiotedge/modules/hello_world/inc/
	install -m 0755 ${WORKDIR}/hello-world-module.sh ${D}${exec_prefix}/src/azureiotedge/modules/hello_world/build.sh

	# Identity Map Module
	install -d ${D}${libdir}/azureiotedge/modules/identitymap
	install -m 0755 ${B}/modules/identitymap/*.so ${D}${libdir}/azureiotedge/modules/identitymap/
	
	install -d ${D}${includedir}/azureiotedge/modules/identitymap
	install -m 0644 ${S}/modules/identitymap/inc/*.h ${D}${includedir}/azureiotedge/modules/identitymap

	install -d ${D}${exec_prefix}/src/azureiotedge/modules/identitymap/src
	install -d ${D}${exec_prefix}/src/azureiotedge/modules/identitymap/inc
	install -m 0644 ${S}/modules/identitymap/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/identitymap/src/
	install -m 0644 ${S}/modules/identitymap/inc/*.h ${D}${exec_prefix}/src/azureiotedge/modules/identitymap/inc/
	install -m 0755 ${WORKDIR}/identitymap-module.sh ${D}${exec_prefix}/src/azureiotedge/modules/identitymap/build.sh

	# IoT Hub Module
	install -d ${D}${libdir}/azureiotedge/modules/iothub
	install -m 0755 ${B}/modules/iothub/*.so ${D}${libdir}/azureiotedge/modules/iothub/

	install -d ${D}${includedir}/azureiotedge/modules/iothub
	install -m 0644 ${S}/modules/iothub/inc/*.h ${D}${includedir}/azureiotedge/modules/iothub

	install -d ${D}${exec_prefix}/src/azureiotedge/modules/iothub/src
	install -d ${D}${exec_prefix}/src/azureiotedge/modules/iothub/inc
	install -m 0644 ${S}/modules/iothub/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/iothub/src/
	install -m 0644 ${S}/modules/iothub/inc/*.h ${D}${exec_prefix}/src/azureiotedge/modules/iothub/inc/
	install -m 0755 ${WORKDIR}/iothub-module.sh ${D}${exec_prefix}/src/azureiotedge/modules/iothub/build.sh

	# Logger Module
	install -d ${D}${libdir}/azureiotedge/modules/logger
	install -m 0755 ${B}/modules/logger/*.so ${D}${libdir}/azureiotedge/modules/logger/

	install -d ${D}${includedir}/azureiotedge/modules/logger
	install -m 0644 ${S}/modules/logger/inc/*.h ${D}${includedir}/azureiotedge/modules/logger

	install -d ${D}${exec_prefix}/src/azureiotedge/modules/logger/src
	install -d ${D}${exec_prefix}/src/azureiotedge/modules/logger/inc
	install -m 0644 ${S}/modules/logger/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/logger/src/
	install -m 0644 ${S}/modules/logger/inc/*.h ${D}${exec_prefix}/src/azureiotedge/modules/logger/inc/
	install -m 0755 ${WORKDIR}/logger-module.sh ${D}${exec_prefix}/src/azureiotedge/modules/logger/build.sh

	# Simulated Device Module
	install -d ${D}${libdir}/azureiotedge/modules/simulated_device
	install -m 0755 ${B}/modules/simulated_device/*.so ${D}${libdir}/azureiotedge/modules/simulated_device/

	install -d ${D}${includedir}/azureiotedge/modules/simulated_device
	install -m 0644 ${S}/modules/simulated_device/inc/*.h ${D}${includedir}/azureiotedge/modules/simulated_device

	install -d ${D}${exec_prefix}/src/azureiotedge/modules/simulated_device/src
	install -d ${D}${exec_prefix}/src/azureiotedge/modules/simulated_device/inc
	install -m 0644 ${S}/modules/simulated_device/src/*.c ${D}${exec_prefix}/src/azureiotedge/modules/simulated_device/src/
	install -m 0644 ${S}/modules/simulated_device/inc/*.h ${D}${exec_prefix}/src/azureiotedge/modules/simulated_device/inc/
	install -m 0755 ${WORKDIR}/simulated-device-module.sh ${D}${exec_prefix}/src/azureiotedge/modules/simulated_device/build.sh

	# Azure Functions Sample
	install -d ${D}${datadir}/azureiotedge/samples/azure_functions
	install -m 0755 ${B}/samples/azure_functions_sample/azure_functions_sample ${D}${datadir}/azureiotedge/samples/azure_functions/azure_functions
	install -m 0644 ${S}/samples/azure_functions_sample/src/azure_functions_lin.json ${D}${datadir}/azureiotedge/samples/azure_functions/azure_functions.json

	install -d ${D}${exec_prefix}/src/azureiotedge/samples/azure_functions/src
	install -m 0644 ${S}/samples/azure_functions_sample/src/*.c ${D}${exec_prefix}/src/azureiotedge/samples/azure_functions/src/
	install -m 0644 ${S}/samples/azure_functions_sample/src/azure_functions_lin.json ${D}${exec_prefix}/src/azureiotedge/samples/azure_functions/src/azure_functions.json
	install -m 0755 ${WORKDIR}/azure-functions-sample.sh ${D}${exec_prefix}/src/azureiotedge/samples/azure_functions/build.sh

	# BLE Gateway Sample
	if [ -e ${B}/samples/ble_gateway/ ]; then
		install -d ${D}${datadir}/azureiotedge/samples/ble_gateway
		install -m 0755 ${B}/samples/ble_gateway/ble_gateway ${D}${datadir}/azureiotedge/samples/ble_gateway/ble_gateway
		install -m 0644 ${B}/samples/ble_gateway/ble_printer/libble_printer.so ${D}${datadir}/azureiotedge/samples/ble_gateway/
		install -m 0644 ${S}/samples/ble_gateway/src/gateway_sample.json ${D}${datadir}/azureiotedge/samples/ble_gateway/ble_gateway.json

		install -d ${D}${exec_prefix}/src/azureiotedge/samples/ble_gateway/src
		install -d ${D}${exec_prefix}/src/azureiotedge/samples/ble_gateway/ble_printer/src
		install -d ${D}${exec_prefix}/src/azureiotedge/samples/ble_gateway/ble_printer/inc
		install -m 0644 ${S}/samples/ble_gateway/src/*.c ${D}${exec_prefix}/src/azureiotedge/samples/ble_gateway/src/
		install -m 0644 ${S}/samples/ble_gateway/src/gateway_sample.json ${D}${exec_prefix}/src/azureiotedge/samples/ble_gateway/src/gateway.json
		install -m 0644 ${S}/samples/ble_gateway/ble_printer/src/*.c ${D}${exec_prefix}/src/azureiotedge/samples/ble_gateway/ble_printer/src/
		install -m 0644 ${S}/samples/ble_gateway/ble_printer/inc/*.h ${D}${exec_prefix}/src/azureiotedge/samples/ble_gateway/ble_printer/inc/
		install -m 0755 ${WORKDIR}/ble-gateway-sample.sh ${D}${exec_prefix}/src/azureiotedge/samples/ble_gateway/build.sh
	fi

	# Dynamically Add Module Sample
	install -d ${D}${datadir}/azureiotedge/samples/dynamically_add_module
	install -m 0755 ${B}/samples/dynamically_add_module_sample/dynamically_add_module_sample ${D}${datadir}/azureiotedge/samples/dynamically_add_module/dynamically_add_module
	install -m 0644 ${S}/samples/dynamically_add_module_sample/src/links_lin.json ${D}${datadir}/azureiotedge/samples/dynamically_add_module/links.json
	install -m 0644 ${S}/samples/dynamically_add_module_sample/src/modules_lin.json ${D}${datadir}/azureiotedge/samples/dynamically_add_module/modules.json

	install -d ${D}${exec_prefix}/src/azureiotedge/samples/dynamically_add_module/src
	install -m 0644 ${S}/samples/dynamically_add_module_sample/src/*.c ${D}${exec_prefix}/src/azureiotedge/samples/dynamically_add_module/src/
	install -m 0644 ${S}/samples/dynamically_add_module_sample/src/links_lin.json ${D}${exec_prefix}/src/azureiotedge/samples/dynamically_add_module/src/links.json
	install -m 0644 ${S}/samples/dynamically_add_module_sample/src/modules_lin.json ${D}${exec_prefix}/src/azureiotedge/samples/dynamically_add_module/src/modules.json
	install -m 0755 ${WORKDIR}/dynamically-add-module-sample.sh ${D}${exec_prefix}/src/azureiotedge/samples/dynamically_add_module/build.sh

	# Hello World Sample
	install -d ${D}${datadir}/azureiotedge/samples/hello_world
	install -m 0755 ${B}/samples/hello_world/hello_world_sample ${D}${datadir}/azureiotedge/samples/hello_world/hello_world
	install -m 0644 ${S}/samples/hello_world/src/hello_world_lin.json ${D}${datadir}/azureiotedge/samples/hello_world/hello_world.json

	install -d ${D}${exec_prefix}/src/azureiotedge/samples/hello_world/src
	install -m 0644 ${S}/samples/hello_world/src/*.c ${D}${exec_prefix}/src/azureiotedge/samples/hello_world/src/
	install -m 0644 ${S}/samples/hello_world/src/hello_world_lin.json ${D}${exec_prefix}/src/azureiotedge/samples/hello_world/src/hello_world.json
	install -m 0755 ${WORKDIR}/hello-world-sample.sh ${D}${exec_prefix}/src/azureiotedge/samples/hello_world/build.sh

	# Native Module Host Sample
	install -d ${D}${datadir}/azureiotedge/samples/native_module_host
	install -m 0755 ${B}/samples/native_module_host_sample/native_host_sample ${D}${datadir}/azureiotedge/samples/native_module_host/native_host
	install -m 0755 ${B}/samples/native_module_host_sample/native_gateway ${D}${datadir}/azureiotedge/samples/native_module_host/native_gateway
	install -m 0644 ${S}/samples/native_module_host_sample/src/native_host_sample_lin.json ${D}${datadir}/azureiotedge/samples/native_module_host/native_host.json

	install -d ${D}${exec_prefix}/src/azureiotedge/samples/native_module_host/src
	install -m 0644 ${S}/samples/native_module_host_sample/src/*.c ${D}${exec_prefix}/src/azureiotedge/samples/native_module_host/src/
	install -m 0644 ${S}/samples/native_module_host_sample/src/native_host_sample_lin.json ${D}${exec_prefix}/src/azureiotedge/samples/native_module_host/src/native_host.json
	install -m 0755 ${WORKDIR}/native-module-host-sample.sh ${D}${exec_prefix}/src/azureiotedge/samples/native_module_host/build.sh

	# Proxy Sample
	install -d ${D}${datadir}/azureiotedge/samples/proxy
	install -m 0755 ${B}/samples/proxy_sample/proxy_sample ${D}${datadir}/azureiotedge/samples/proxy/proxy
	install -m 0755 ${B}/samples/proxy_sample/proxy_sample_remote ${D}${datadir}/azureiotedge/samples/proxy/proxy_remote
	install -m 0644 ${S}/samples/proxy_sample/src/proxy_sample_lin.json ${D}${datadir}/azureiotedge/samples/proxy/proxy.json

	install -d ${D}${exec_prefix}/src/azureiotedge/samples/proxy/src
	install -m 0644 ${S}/samples/proxy_sample/src/*.c ${D}${exec_prefix}/src/azureiotedge/samples/proxy/src/
	install -m 0644 ${S}/samples/proxy_sample/src/proxy_sample_lin.json ${D}${exec_prefix}/src/azureiotedge/samples/proxy/src/proxy.json
	install -m 0755 ${WORKDIR}/proxy-sample.sh ${D}${exec_prefix}/src/azureiotedge/samples/proxy/build.sh

	# Simulated Device Cloud Upload Sample
	install -d ${D}${datadir}/azureiotedge/samples/simulated_device_cloud_upload
	install -m 0755 ${B}/samples/simulated_device_cloud_upload/simulated_device_cloud_upload_sample ${D}${datadir}/azureiotedge/samples/simulated_device_cloud_upload/simulated_device_cloud_upload
	install -m 0644 ${S}/samples/simulated_device_cloud_upload/src/simulated_device_cloud_upload_lin.json ${D}${datadir}/azureiotedge/samples/simulated_device_cloud_upload/simulated_device_cloud_upload.json

	install -d ${D}${exec_prefix}/src/azureiotedge/samples/simulated_device_cloud_upload/src
	install -d ${D}${exec_prefix}/src/azureiotedge/samples/simulated_device_cloud_upload/inc
	install -m 0644 ${S}/samples/simulated_device_cloud_upload/src/*.c ${D}${exec_prefix}/src/azureiotedge/samples/simulated_device_cloud_upload/src/
	install -m 0644 ${S}/samples/simulated_device_cloud_upload/inc/*.h ${D}${exec_prefix}/src/azureiotedge/samples/simulated_device_cloud_upload/inc/
	install -m 0644 ${S}/samples/simulated_device_cloud_upload/src/simulated_device_cloud_upload_lin.json ${D}${exec_prefix}/src/azureiotedge/samples/simulated_device_cloud_upload/src/simulated_device_cloud_upload.json
	install -m 0755 ${WORKDIR}/simulated-device-cloud-upload-sample.sh ${D}${exec_prefix}/src/azureiotedge/samples/simulated_device_cloud_upload/build.sh

	# Java
	if ${@bb.utils.contains('PACKAGECONFIG','java','true','false',d)}; then
		install -d ${D}${libdir}/azureiotedge/bindings/java
    	install -m 0755 ${B}/bindings/java/*.so ${D}${libdir}/azureiotedge/bindings/java/
    	
    	install -d ${D}${libdir}
    	install -m 0755 ${B}/proxy/gateway/java/*.so ${D}${libdir}
	fi

	# .NET Core
	if ${@bb.utils.contains('PACKAGECONFIG','dotnetcore','true','false',d)}; then
		install -d ${D}${libdir}/azureiotedge/bindings/dotnetcore
    	install -m 0755 ${B}/bindings/dotnetcore/*.so ${D}${libdir}/azureiotedge/bindings/dotnetcore/
	fi
}

RDEPENDS_${PN} = "\
	glib-2.0 \
	curl \
"
FILES_${PN} = "\
	${libdir}/libgateway.so \
	${libdir}/libproxy_gateway.so \
	${libdir}/libnative_module_host.so \
"

RDEPENDS_${PN}-dev = "\
	azure-iot-sdk-c-dev \
	nanomsg-dev \
	glib-2.0-dev \
"
FILES_${PN}-dev = "\
	${includedir}/azureiotedge \
"

FILES_${PN}-dbg = "\
    ${libdir}/.debug \
    ${libdir}/azureiotedge/bindings/*/.debug \
    ${libdir}/azureiotedge/modules/*/.debug \
    ${datadir}/azureiotedge/samples/*/.debug \
    ${exec_prefix}/src/debug \
"

FILES_${PN}-modules = "\
	${libdir}/azureiotedge/modules/*/*.so \
"

FILES_${PN}-modules-src = "\
	${exec_prefix}/src/azureiotedge/modules \
"

RDEPENDS_${PN}-samples = "azure-iot-edge-modules"
FILES_${PN}-samples = "\
	${datadir}/azureiotedge/samples/*/* \
"

FILES_${PN}-samples-src = "\
	${exec_prefix}/src/azureiotedge/samples/ \
"

FILES_${PN}-dotnetcore = "\
	${libdir}/azureiotedge/bindings/dotnetcore/*.so \
"

RDEPENDS_${PN}-java = "\
    java2-runtime \
"
FILES_${PN}-java = "\
    ${libdir}/libjava_nanomsg.so \
	${libdir}/azureiotedge/bindings/java/*.so \
"

RRECOMMENDS_azure-iot-edge-dev[nodeprrecs] = "1"

INSANE_SKIP_${PN} += "rpaths"
INSANE_SKIP_${PN}-dotnetcore += "rpaths"
INSANE_SKIP_${PN}-java += "rpaths file-rdeps"
INSANE_SKIP_${PN}-modules += "rpaths"
INSANE_SKIP_${PN}-samples += "rpaths libdir"
