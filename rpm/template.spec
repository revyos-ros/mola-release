%bcond_without tests
%bcond_without weak_deps

%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')
%global __provides_exclude_from ^/opt/ros/humble/.*$
%global __requires_exclude_from ^/opt/ros/humble/.*$

Name:           ros-humble-mola-input-euroc-dataset
Version:        1.0.4
Release:        1%{?dist}%{?release_suffix}
Summary:        ROS mola_input_euroc_dataset package

License:        BSD
URL:            https://github.com/MOLAorg/mola/tree/develop/mola_input_euroc_dataset
Source0:        %{name}-%{version}.tar.gz

Requires:       ros-humble-mola-common
Requires:       ros-humble-mola-kernel
Requires:       ros-humble-mrpt2
Requires:       ros-humble-ros-workspace
BuildRequires:  cmake3
BuildRequires:  ros-humble-mola-common
BuildRequires:  ros-humble-mola-kernel
BuildRequires:  ros-humble-mrpt2
BuildRequires:  ros-humble-ros-workspace
Provides:       %{name}-devel = %{version}-%{release}
Provides:       %{name}-doc = %{version}-%{release}
Provides:       %{name}-runtime = %{version}-%{release}

%description
Offline RawDataSource from EUROC SLAM datasets

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
* Tue May 14 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.4-1
- Autogenerated by Bloom

* Mon Apr 22 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.3-1
- Autogenerated by Bloom

* Thu Apr 04 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.2-1
- Autogenerated by Bloom

* Thu Mar 28 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.1-1
- Autogenerated by Bloom

* Tue Mar 19 2024 Jose-Luis Blanco-Claraco <joseluisblancoc@gmail.com> - 1.0.0-1
- Autogenerated by Bloom

