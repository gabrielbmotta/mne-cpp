---
title: Overview
parent: Home
nav_order: 1
---
# Overview

MNE-CPP can be used to build standalone software applications, which offer a variety of neuroscientific analysis tools and are easy to use. MNE-CPP provides a cross-platform library which allows the processing of MEG/EEG data. It can be used to develop cross-platform applications for Windows, Linux and Mac.

Besides the basic library (MNE Lib) we deliver a number of applications, e.g., an acquisition and real-time processing (MNE Scan), as well as an offline processing application (MNE Analyze). MNE-CPP is open source licensed under BSD (clause 3). MNE-CPP has two external dependencies, namely [Qt](https://www.qt.io/){:target="_blank" rel="noopener"} and [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page){:target="_blank" rel="noopener"}. 

We are targeting two user groups:
* Front-End users with little or no coding background. These users are interested in straight-forward setups and applications with an easy to use GUI, such as MNE Scan and MNE Analyze. For more information see our [Learn section](pages/learn/learn.md).
* Back-End users who are experienced with programming in C++ and want to use MNE-CPP functionalities to build their own applications. For more information see our [Development section](pages/development/development.md). 

MNE-CPP is a community project. Contributions by new developers are always welcomed and greatly appreciated. If you are planning to contribute to MNE-CPP our [contributor page](pages/contribute/contribute.md) is a good point to start.

Currently, MNE-CPP's development efforts can be divided into the following projects:

| ![MNELib](images/icon_mne-lib_256x256.png) | **MNE Lib** describes the core libraries provided by the MNE-CPP project. MNE Scan and MNE Analyze are solely based on these libraries. |
| ![MNEScan](images/icon_mne_scan_256x256.png) | **MNE Scan** (former MNE-X) is an open-source and cross platform application built with tools provided by the MNE-CPP framework. Currently, it is in active clinical use at the Boston Children’s Hospital. With its acquisition and algorithmic plugin architecture it is able to acquire and process data of several MEG/EEG systems (TMSI Refa, eegosports, gtec, brainamp, LSL, VectorView, BabyMEG) in real-time. |
| ![MNEAnalyze](images/icon_mne-analyze_256x256.png) | **MNE Analyze** is currently in an early development state. When finished it will provide an easy to use GUI for sensor and source level analysis. |