%bcond_without tests
%bcond_without weak_deps

%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')
%global __provides_exclude_from ^/opt/ros/jazzy/.*$
%global __requires_exclude_from ^/opt/ros/jazzy/.*$

Name:           ros-jazzy-mola-navstate-fuse
Version:        1.2.0
Release:        1%{?dist}%{?release_suffix}
Summary:        ROS mola_navstate_fuse package

License:        GPLv3
URL:            https://github.com/MOLAorg/mola/tree/develop/mola_navstate_fuse
Source0:        %{name}-%{version}.tar.gz

Requires:       ros-jazzy-mola-common
Requires:       ros-jazzy-mola-imu-preintegration
Requires:       ros-jazzy-mola-kernel
Requires:       ros-jazzy-mrpt-libobs
Requires:       ros-jazzy-ros-workspace
BuildRequires:  cmake3
BuildRequires:  ros-jazzy-mola-common
BuildRequires:  ros-jazzy-mola-imu-preintegration
BuildRequires:  ros-jazzy-mola-kernel
BuildRequires:  ros-jazzy-mrpt-libobs
BuildRequires:  ros-jazzy-ros-workspace
Provides:       %{name}-devel = %{version}-%{release}
Provides:       %{name}-doc = %{version}-%{release}
Provides:       %{name}-runtime = %{version}-%{release}

%description
SE(3) pose and twist path data fusion estimator

%prep
%autosetup -p1

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/jazzy/setup.sh" ]; then . "/opt/ros/jazzy/setup.sh"; fi
mkdir -p .obj-%{_target_platform} && cd .obj-%{_target_platform}
%cmake3 \
    -UINCLUDE_INSTALL_DIR \
    -ULIB_INSTALL_DIR \
    -USYSCONF_INSTALL_DIR \
    -USHARE_INSTALL_PREFIX \
    -ULIB_SUFFIX \
    -DCMAKE_INSTALL_PREFIX="/opt/ros/jazzy" \
    -DCMAKE_PREFIX_PATH="/opt/ros/jazzy" \
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
if [ -f "/opt/ros/jazzy/setup.sh" ]; then . "/opt/ros/jazzy/setup.sh"; fi
%make_install -C .obj-%{_target_platform}

%if 0%{?with_tests}
%check
# Look for a Makefile target with a name indicating that it runs tests
TEST_TARGET=$(%__make -qp -C .obj-%{_target_platform} | sed "s/^\(test\|check\):.*/\\1/;t f;d;:f;q0")
if [ -n "$TEST_TARGET" ]; then
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/jazzy/setup.sh" ]; then . "/opt/ros/jazzy/setup.sh"; fi
CTEST_OUTPUT_ON_FAILURE=1 \
    %make_build -C .obj-%{_target_platform} $TEST_TARGET || echo "RPM TESTS FAILED"
else echo "RPM TESTS SKIPPED"; fi
%endif

%files
%license LICENSE
/opt/ros/jazzy

%changelog
* Thu Sep 26 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.2.0-1
- Autogenerated by Bloom

* Tue May 28 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.5-1
- Autogenerated by Bloom

* Thu Apr 18 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.2-2
- Autogenerated by Bloom

* Thu Apr 04 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.2-1
- Autogenerated by Bloom

* Thu Mar 28 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.1-1
- Autogenerated by Bloom

* Tue Mar 19 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.0-1
- Autogenerated by Bloom

