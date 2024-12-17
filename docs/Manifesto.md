Meadow.OS - A modern Internet-ready operating system for Meadow devices (DRAFT)
---

Definitions
--
* **OS** - Meadow.OS itself
* **Meadow App** - A software application created for the Meadow platform
* **App Developer** - The entity developing the application making use the OS 
* **Meadow.Cloud** - The Internet service providing update, report and control functionality for Meadow device fleets

Purpose
--
* An Internet-enabled software application platform for users of Wilderness Lab's Meadow boards
* A comprehensive API to the Meadow board's features and functionality
* Always-available self-update functionality for OS and Meadow App
* Developer-friendly operation modes that enable rapid Meadow App development


Top-Level Components And Their Role
-- 

* **Meadow.OS Bootloader**: System Update & Rollback
* **NuttX**: Meadow board initialization * OS kernel
* **Mono**: .NET App Runtime
* **HCOM**: Userspace startup manager, app and firmware updates
* **mbedTLS**: Internet Cryptography library
* **Meadow.Cloud Daemon**: health & error reporting, system & app update channel


Requirements
--
Security
--

* The OS should provide transparently secure internet communication to the Meadow App
* The OS should use secure internet communications to connect and communicate with Meadow.Cloud

Availability
--
* If the OS is not :
    * Initializing
    * Updating/Rolling back an update

    the app MUST always be running (with the exception of developer-use "stopped app" modes)


Reliability
--

* A user app executing valid code MUST NOT crash
* A user app executing invalid code MUST NOT crash error reporting


Failure & Recovery    
--

Failures in a run-time software system are either:

 **recoverable** : Some executed code can restore the system to a stable state. (e.g. A peripheral and its driver need to be reset)
 
or **non-recoverable**: There is no method (that the engineer can see) that the system can use to restore the system to stable state. (e.g. most Out-Of-Memory errors in user apps)

Failures are also either:

**transient** : Re-trying the operation *may* succeed

or **persistent** : The operation will never work under the current conditions.


* An unrecoverable system crash should be treated as transient and  **RESET** the device.
* Any unrecoverable errors should be reported to the Meadow.Cloud daemon after reset.
* If recoverable failure has only a single recovery method (to the engineers' understanding), this method MUST be implemented as part of the System.
* If a recoverable failure has multiple recovery methods, the System MUST allow the App Developer to implement programmatic recovery as they see fit.


Updateability
--

TODO

Low Power Use
--

TODO