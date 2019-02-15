/*
  Copyright 2019 Mike Krus

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#include "timeularmanager.h"

#include <QDebug>
#include <QBluetoothUuid>

namespace {
    const QBluetoothUuid zeiOrientationService(QLatin1Literal("{c7e70010-c847-11e6-8175-8c89a55d403c}"));
    const QBluetoothUuid zeiOrientationCharacteristic(QLatin1Literal("{c7e70012-c847-11e6-8175-8c89a55d403c}"));
}

TimeularManager::TimeularManager(QObject *parent)
    : QObject(parent)
{
    m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_deviceDiscoveryAgent->setLowEnergyDiscoveryTimeout(5000);

    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            [this]() {
                if (!m_service)
                    this->startDiscovery();
            });
    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &TimeularManager::deviceDiscovered);
}

TimeularManager::~TimeularManager()
{
    delete m_service;
}

TimeularManager::Orientation TimeularManager::orientation() const
{
    return m_orientation;
}

TimeularManager::Status TimeularManager::status() const
{
    return m_status;
}

void TimeularManager::setStatus(Status status)
{
    if (status != m_status) {
        m_status = status;
        emit statusChanged(m_status);
    }
}

void TimeularManager::startDiscovery()
{
    if (m_status != Disconneted)
        return;

    qDebug() << "Starting Discovery";
    setStatus(Connecting);
    m_deviceDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void TimeularManager::deviceDiscovered(const QBluetoothDeviceInfo &info)
{
    if (m_status != Connecting)
        return;
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration))
        return;

    if (info.name() == QLatin1Literal("Timeular ZEI")) {
        qDebug() << "Connecting to device";
        if (!m_controller) {
            m_controller = QLowEnergyController::createCentral(info, this);
            m_controller ->setRemoteAddressType(QLowEnergyController::RandomAddress);

            connect(m_controller, &QLowEnergyController::connected,
                    this, &TimeularManager::deviceConnected);
            connect(m_controller, QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error),
                    this, &TimeularManager::errorReceived);
            connect(m_controller, &QLowEnergyController::disconnected,
                    this, &TimeularManager::deviceDisconnected);
            connect(m_controller, &QLowEnergyController::serviceDiscovered,
                    this, &TimeularManager::addLowEnergyService);
            connect(m_controller, &QLowEnergyController::discoveryFinished,
                    this, &TimeularManager::serviceScanDone);
        }

        m_controller->connectToDevice();
    }
}

void TimeularManager::deviceConnected()
{
    m_controller->discoverServices();
}

void TimeularManager::deviceDisconnected()
{
    setStatus(Disconneted);
    delete m_service;
    m_service = nullptr;
    qDebug() << "Device Disconneted";
}

void TimeularManager::errorReceived(QLowEnergyController::Error /*error*/)
{
    qWarning() << "Error: " << m_controller->errorString();
}

void TimeularManager::serviceScanDone()
{
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }

    if (m_serviceDiscovered)
        m_service = m_controller->createServiceObject(QBluetoothUuid(zeiOrientationService), this);

    if (m_service) {
        connect(m_service, &QLowEnergyService::stateChanged,
                this, &TimeularManager::serviceStateChanged);
        connect(m_service, &QLowEnergyService::characteristicChanged,
                this, &TimeularManager::deviceDataChanged);
        connect(m_service, &QLowEnergyService::descriptorWritten,
                this, &TimeularManager::confirmedDescriptorWrite);
        m_service->discoverDetails();
    } else {
        qDebug() << "Service not found";
    }
}

void TimeularManager::confirmedDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value)
{
    if (d.isValid() && d == m_notificationDesc && value == QByteArray::fromHex("0000")) {
        //disabled notifications -> assume disconnect intent
        m_controller->disconnectFromDevice();
        delete m_service;
        m_service = nullptr;
    }
}

void TimeularManager::addLowEnergyService(const QBluetoothUuid &serviceUuid)
{
    if (serviceUuid == QBluetoothUuid(zeiOrientationService)) {
        m_serviceDiscovered = true;
    }
}

void TimeularManager::serviceStateChanged(QLowEnergyService::ServiceState newState)
{
    switch (newState) {
    case QLowEnergyService::DiscoveringServices:
        break;
    case QLowEnergyService::ServiceDiscovered: {
        const QLowEnergyCharacteristic orientationChar = m_service->characteristic(QBluetoothUuid(zeiOrientationCharacteristic));
        if (!orientationChar.isValid()) {
            qDebug() << "Orientation data not found";
            setStatus(Disconneted);
            break;
        }

        m_notificationDesc = orientationChar.descriptor(QBluetoothUuid(QLatin1String("{00002902-0000-1000-8000-00805f9b34fb}")));
        if (m_notificationDesc.isValid()) {
            qDebug() << "Device Connected";
            setStatus(Connected);
            m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));
        } else {
            setStatus(Disconneted);
        }

        break;
    }
    default:
        //nothing for now
        break;
    }
}

void TimeularManager::deviceDataChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if (c.uuid() != QBluetoothUuid(zeiOrientationCharacteristic))
        return;

    auto data = reinterpret_cast<const quint8 *>(value.constData());
    quint8 orientation = *data;
    qDebug() << "Orientation" << orientation;
    if (orientation > 8)
        orientation = 0;
    if(orientation != m_orientation) {
        m_orientation = static_cast<Orientation>(orientation);
        emit orientationChanged(m_orientation);
    }
}
