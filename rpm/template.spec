Name:           ros-indigo-dataspeed-can-msg-filters
Version:        1.0.12
Release:        0%{?dist}
Summary:        ROS dataspeed_can_msg_filters package

Group:          Development/Libraries
License:        BSD
URL:            http://dataspeedinc.com
Source0:        %{name}-%{version}.tar.gz

Requires:       ros-indigo-can-msgs
Requires:       ros-indigo-roscpp
BuildRequires:  ros-indigo-can-msgs
BuildRequires:  ros-indigo-catkin
BuildRequires:  ros-indigo-roscpp

%description
Time synchronize multiple CAN messages to get a single callback

%prep
%setup -q

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree that was dropped by catkin, and source it.  It will
# set things like CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/indigo/setup.sh" ]; then . "/opt/ros/indigo/setup.sh"; fi
mkdir -p obj-%{_target_platform} && cd obj-%{_target_platform}
%cmake .. \
        -UINCLUDE_INSTALL_DIR \
        -ULIB_INSTALL_DIR \
        -USYSCONF_INSTALL_DIR \
        -USHARE_INSTALL_PREFIX \
        -ULIB_SUFFIX \
        -DCMAKE_INSTALL_LIBDIR="lib" \
        -DCMAKE_INSTALL_PREFIX="/opt/ros/indigo" \
        -DCMAKE_PREFIX_PATH="/opt/ros/indigo" \
        -DSETUPTOOLS_DEB_LAYOUT=OFF \
        -DCATKIN_BUILD_BINARY_PACKAGE="1" \

make %{?_smp_mflags}

%install
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree that was dropped by catkin, and source it.  It will
# set things like CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/indigo/setup.sh" ]; then . "/opt/ros/indigo/setup.sh"; fi
cd obj-%{_target_platform}
make %{?_smp_mflags} install DESTDIR=%{buildroot}

%files
/opt/ros/indigo

%changelog
* Wed Dec 19 2018 Kevin Hallenbeck <khallenbeck@dataspeedinc.com> - 1.0.12-0
- Autogenerated by Bloom

* Thu Sep 06 2018 Kevin Hallenbeck <khallenbeck@dataspeedinc.com> - 1.0.11-0
- Autogenerated by Bloom

* Wed Jun 13 2018 Kevin Hallenbeck <khallenbeck@dataspeedinc.com> - 1.0.10-0
- Autogenerated by Bloom

* Tue Jun 12 2018 Kevin Hallenbeck <khallenbeck@dataspeedinc.com> - 1.0.9-0
- Autogenerated by Bloom

