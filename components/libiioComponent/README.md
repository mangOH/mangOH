# libiio Legato Component

[libiio](https://wiki.analog.com/resources/tools-software/linux-software/libiio) is a library for
interacting with iio devices. This repository provides a Legato component which builds and bundles
libiio. A [mangOH fork](https://github.com/mangOH/libiio) of the [libiio git repository](
https://github.com/analogdevicesinc/libiio) is included as a git submodule.

To use this component, the Component.cdef of the component wishing to use libiio should be modified
as follows:
```
requires:
{
    component:
    {
        ${MANGOH_ROOT}/components/libiioComponent
    }
    lib:
    {
        libiio.so
        libiio.so.0
        libiio.so.0.18
    }
}
```
