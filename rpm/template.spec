%bcond_without tests
%bcond_without weak_deps

%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')
%global __provides_exclude_from ^/opt/ros/rolling/.*$
%global __requires_exclude_from ^/opt/ros/rolling/.*$

Name:           ros-rolling-mola
Version:        1.0.5
Release:        1%{?dist}%{?release_suffix}
Summary:        ROS mola package

License:        BSD
Source0:        %{name}-%{version}.tar.gz

Requires:       ros-rolling-ament-lint-auto
Requires:       ros-rolling-ament-lint-common
Requires:       ros-rolling-kitti-metrics-eval
Requires:       ros-rolling-mola-bridge-ros2
Requires:       ros-rolling-mola-demos
Requires:       ros-rolling-mola-imu-preintegration
Requires:       ros-rolling-mola-input-euroc-dataset
Requires:       ros-rolling-mola-input-kitti-dataset
Requires:       ros-rolling-mola-input-kitti360-dataset
Requires:       ros-rolling-mola-input-mulran-dataset
Requires:       ros-rolling-mola-input-paris-luco-dataset
Requires:       ros-rolling-mola-input-rawlog
Requires:       ros-rolling-mola-input-rosbag2
Requires:       ros-rolling-mola-kernel
Requires:       ros-rolling-mola-launcher
Requires:       ros-rolling-mola-metric-maps
Requires:       ros-rolling-mola-navstate-fuse
Requires:       ros-rolling-mola-pose-list
Requires:       ros-rolling-mola-relocalization
Requires:       ros-rolling-mola-traj-tools
Requires:       ros-rolling-mola-viz
Requires:       ros-rolling-mola-yaml
Requires:       ros-rolling-ros-workspace
BuildRequires:  ros-rolling-ament-cmake
BuildRequires:  ros-rolling-ament-lint-auto
BuildRequires:  ros-rolling-ament-lint-common
BuildRequires:  ros-rolling-kitti-metrics-eval
BuildRequires:  ros-rolling-mola-bridge-ros2
BuildRequires:  ros-rolling-mola-demos
BuildRequires:  ros-rolling-mola-imu-preintegration
BuildRequires:  ros-rolling-mola-input-euroc-dataset
BuildRequires:  ros-rolling-mola-input-kitti-dataset
BuildRequires:  ros-rolling-mola-input-kitti360-dataset
BuildRequires:  ros-rolling-mola-input-mulran-dataset
BuildRequires:  ros-rolling-mola-input-paris-luco-dataset
BuildRequires:  ros-rolling-mola-input-rawlog
BuildRequires:  ros-rolling-mola-input-rosbag2
BuildRequires:  ros-rolling-mola-kernel
BuildRequires:  ros-rolling-mola-launcher
BuildRequires:  ros-rolling-mola-metric-maps
BuildRequires:  ros-rolling-mola-navstate-fuse
BuildRequires:  ros-rolling-mola-pose-list
BuildRequires:  ros-rolling-mola-relocalization
BuildRequires:  ros-rolling-mola-traj-tools
BuildRequires:  ros-rolling-mola-viz
BuildRequires:  ros-rolling-mola-yaml
BuildRequires:  ros-rolling-ros-workspace
Provides:       %{name}-devel = %{version}-%{release}
Provides:       %{name}-doc = %{version}-%{release}
Provides:       %{name}-runtime = %{version}-%{release}

%description
Metapackage with all core open-sourced MOLA packages.

%prep
%autosetup -p1

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/rolling/setup.sh" ]; then . "/opt/ros/rolling/setup.sh"; fi
mkdir -p .obj-%{_target_platform} && cd .obj-%{_target_platform}
%cmake3 \
    -UINCLUDE_INSTALL_DIR \
    -ULIB_INSTALL_DIR \
    -USYSCONF_INSTALL_DIR \
    -USHARE_INSTALL_PREFIX \
    -ULIB_SUFFIX \
    -DCMAKE_INSTALL_PREFIX="/opt/ros/rolling" \
    -DAMENT_PREFIX_PATH="/opt/ros/rolling" \
    -DCMAKE_PREFIX_PATH="/opt/ros/rolling" \
    -DSETUPTOOLS_DEB_LAYOUT=OFF \
%if !0%{?with_tests}
    -DBUILD_TESTING=OFF \
%endif
    ..

%make_build

%install
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/rolling/setup.sh" ]; then . "/opt/ros/rolling/setup.sh"; fi
%make_install -C .obj-%{_target_platform}

%if 0%{?with_tests}
%check
# Look for a Makefile target with a name indicating that it runs tests
TEST_TARGET=$(%__make -qp -C .obj-%{_target_platform} | sed "s/^\(test\|check\):.*/\\1/;t f;d;:f;q0")
if [ -n "$TEST_TARGET" ]; then
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/rolling/setup.sh" ]; then . "/opt/ros/rolling/setup.sh"; fi
CTEST_OUTPUT_ON_FAILURE=1 \
    %make_build -C .obj-%{_target_platform} $TEST_TARGET || echo "RPM TESTS FAILED"
else echo "RPM TESTS SKIPPED"; fi
%endif

%files
%license LICENSE
/opt/ros/rolling

%changelog
* Tue May 28 2024 Jose-Luis Blanco-Claraco <jlblanco@ual.es> - 1.0.5-1
- Autogenerated by Bloom

* Tue May 14 2024 Jose-Luis Blanco-Claraco <jlblanco@ual.es> - 1.0.4-1
- Autogenerated by Bloom

* Mon Apr 22 2024 Jose-Luis Blanco-Claraco <jlblanco@ual.es> - 1.0.3-1
- Autogenerated by Bloom

* Thu Apr 04 2024 Jose-Luis Blanco-Claraco <jlblanco@ual.es> - 1.0.2-1
- Autogenerated by Bloom

* Thu Mar 28 2024 Jose-Luis Blanco-Claraco <jlblanco@ual.es> - 1.0.1-1
- Autogenerated by Bloom

* Tue Mar 19 2024 Jose-Luis Blanco-Claraco <jlblanco@ual.es> - 1.0.0-1
- Autogenerated by Bloom

