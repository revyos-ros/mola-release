%bcond_without tests
%bcond_without weak_deps

%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')
%global __provides_exclude_from ^/opt/ros/humble/.*$
%global __requires_exclude_from ^/opt/ros/humble/.*$

Name:           ros-humble-mola
Version:        1.0.5
Release:        1%{?dist}%{?release_suffix}
Summary:        ROS mola package

License:        BSD
Source0:        %{name}-%{version}.tar.gz

Requires:       ros-humble-ament-lint-auto
Requires:       ros-humble-ament-lint-common
Requires:       ros-humble-kitti-metrics-eval
Requires:       ros-humble-mola-bridge-ros2
Requires:       ros-humble-mola-demos
Requires:       ros-humble-mola-imu-preintegration
Requires:       ros-humble-mola-input-euroc-dataset
Requires:       ros-humble-mola-input-kitti-dataset
Requires:       ros-humble-mola-input-kitti360-dataset
Requires:       ros-humble-mola-input-mulran-dataset
Requires:       ros-humble-mola-input-paris-luco-dataset
Requires:       ros-humble-mola-input-rawlog
Requires:       ros-humble-mola-input-rosbag2
Requires:       ros-humble-mola-kernel
Requires:       ros-humble-mola-launcher
Requires:       ros-humble-mola-metric-maps
Requires:       ros-humble-mola-navstate-fuse
Requires:       ros-humble-mola-pose-list
Requires:       ros-humble-mola-relocalization
Requires:       ros-humble-mola-traj-tools
Requires:       ros-humble-mola-viz
Requires:       ros-humble-mola-yaml
Requires:       ros-humble-ros-workspace
BuildRequires:  ros-humble-ament-cmake
BuildRequires:  ros-humble-ament-lint-auto
BuildRequires:  ros-humble-ament-lint-common
BuildRequires:  ros-humble-kitti-metrics-eval
BuildRequires:  ros-humble-mola-bridge-ros2
BuildRequires:  ros-humble-mola-demos
BuildRequires:  ros-humble-mola-imu-preintegration
BuildRequires:  ros-humble-mola-input-euroc-dataset
BuildRequires:  ros-humble-mola-input-kitti-dataset
BuildRequires:  ros-humble-mola-input-kitti360-dataset
BuildRequires:  ros-humble-mola-input-mulran-dataset
BuildRequires:  ros-humble-mola-input-paris-luco-dataset
BuildRequires:  ros-humble-mola-input-rawlog
BuildRequires:  ros-humble-mola-input-rosbag2
BuildRequires:  ros-humble-mola-kernel
BuildRequires:  ros-humble-mola-launcher
BuildRequires:  ros-humble-mola-metric-maps
BuildRequires:  ros-humble-mola-navstate-fuse
BuildRequires:  ros-humble-mola-pose-list
BuildRequires:  ros-humble-mola-relocalization
BuildRequires:  ros-humble-mola-traj-tools
BuildRequires:  ros-humble-mola-viz
BuildRequires:  ros-humble-mola-yaml
BuildRequires:  ros-humble-ros-workspace
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
if [ -f "/opt/ros/humble/setup.sh" ]; then . "/opt/ros/humble/setup.sh"; fi
mkdir -p .obj-%{_target_platform} && cd .obj-%{_target_platform}
%cmake3 \
    -UINCLUDE_INSTALL_DIR \
    -ULIB_INSTALL_DIR \
    -USYSCONF_INSTALL_DIR \
    -USHARE_INSTALL_PREFIX \
    -ULIB_SUFFIX \
    -DCMAKE_INSTALL_PREFIX="/opt/ros/humble" \
    -DAMENT_PREFIX_PATH="/opt/ros/humble" \
    -DCMAKE_PREFIX_PATH="/opt/ros/humble" \
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
if [ -f "/opt/ros/humble/setup.sh" ]; then . "/opt/ros/humble/setup.sh"; fi
%make_install -C .obj-%{_target_platform}

%if 0%{?with_tests}
%check
# Look for a Makefile target with a name indicating that it runs tests
TEST_TARGET=$(%__make -qp -C .obj-%{_target_platform} | sed "s/^\(test\|check\):.*/\\1/;t f;d;:f;q0")
if [ -n "$TEST_TARGET" ]; then
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/humble/setup.sh" ]; then . "/opt/ros/humble/setup.sh"; fi
CTEST_OUTPUT_ON_FAILURE=1 \
    %make_build -C .obj-%{_target_platform} $TEST_TARGET || echo "RPM TESTS FAILED"
else echo "RPM TESTS SKIPPED"; fi
%endif

%files
%license LICENSE
/opt/ros/humble

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

