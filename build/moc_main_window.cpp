/****************************************************************************
** Meta object code from reading C++ file 'main_window.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../src/main_window.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'main_window.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10MainWindowE = QtMocHelpers::stringData(
    "MainWindow",
    "onRefreshDevices",
    "",
    "onStartStop",
    "onWaveChanged",
    "index",
    "onFreqChanged",
    "v",
    "onAmpSlider",
    "onGenToggled",
    "on",
    "onMonitorToggled",
    "onSourceChanged",
    "onTimebaseChanged",
    "onGainChanged",
    "onFreezeToggled",
    "tick"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10MainWindowE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   86,    2, 0x08,    1 /* Private */,
       3,    0,   87,    2, 0x08,    2 /* Private */,
       4,    1,   88,    2, 0x08,    3 /* Private */,
       6,    1,   91,    2, 0x08,    5 /* Private */,
       8,    1,   94,    2, 0x08,    7 /* Private */,
       9,    1,   97,    2, 0x08,    9 /* Private */,
      11,    1,  100,    2, 0x08,   11 /* Private */,
      12,    1,  103,    2, 0x08,   13 /* Private */,
      13,    1,  106,    2, 0x08,   15 /* Private */,
      14,    1,  109,    2, 0x08,   17 /* Private */,
      15,    1,  112,    2, 0x08,   19 /* Private */,
      16,    0,  115,    2, 0x08,   21 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Double,    7,
    QMetaType::Void, QMetaType::Int,    7,
    QMetaType::Void, QMetaType::Bool,   10,
    QMetaType::Void, QMetaType::Bool,   10,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Bool,   10,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_ZN10MainWindowE.offsetsAndSizes,
    qt_meta_data_ZN10MainWindowE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10MainWindowE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'onRefreshDevices'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onStartStop'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onWaveChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onFreqChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'onAmpSlider'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onGenToggled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onMonitorToggled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onSourceChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onTimebaseChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onGainChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onFreezeToggled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'tick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onRefreshDevices(); break;
        case 1: _t->onStartStop(); break;
        case 2: _t->onWaveChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->onFreqChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 4: _t->onAmpSlider((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->onGenToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->onMonitorToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->onSourceChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->onTimebaseChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->onGainChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 10: _t->onFreezeToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->tick(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10MainWindowE.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}
QT_WARNING_POP
