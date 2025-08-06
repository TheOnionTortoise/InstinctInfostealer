# Instinct Infostealer

![download (1)](https://github.com/user-attachments/assets/75ae6c08-d5f5-44d4-945d-2ef8317c8436)


## Overview
**Instinct Infostealer** is a **Proof of Concept (PoC)** demonstrating how **UI Automation** can be abused by threat actors to harvest credentials from browsers while evading Endpoint Detection and Response (EDR) systems. This tool leverages Microsoft's **UI Automation** framework to monitor and interact with web browser elements, focusing on password fields and form inputs.

The primary goal of this project is to showcase the potential risks of UI Automation when misused and to raise awareness about the importance of securing browser interactions against such attacks.


---

<img width="1249" height="515" alt="image (20)" src="https://github.com/user-attachments/assets/8ac93bfc-0b12-4ca6-a695-3675657cd2e5" />


## Features
1. **Credential Harvesting**:
   - Monitors active browser windows for sensitive form inputs (e.g., password fields).
   - Extracts credentials using UI Automation without triggering conventional EDR systems.

2. **Active Browser Monitoring**:
   - Detects supported browsers (Google Chrome and Mozilla Firefox) and processes their active windows.
   - Tracks document titles and form contents in real time.

3. **Password Field Manipulation**:
   - Detects password field interactions and intelligently handles "Show Password" functionality.
   - Simulates mouse clicks to reveal password values when "Show Password" buttons or icons are present.

4. **Form Input Tracking**:
   - Reports changes to form inputs, such as usernames and passwords, while masking sensitive data for demonstration purposes.

5. **UI Automation Techniques**:
   - Utilizes Microsoft's **UI Automation** APIs for interacting with browser elements.
   - Handles complex scenarios, such as detecting focus changes and simulating user interactions.

6. **Evading Detection**:
   - Operates within the boundaries of legitimate UI Automation APIs, making it challenging for EDR systems to identify as malicious activity.

---

## Requirements
- **Operating System**: Windows (supports Windows 10 or later).
- **Language**: C++.
- **Frameworks and Libraries**:
  - **Microsoft UI Automation** (`uiautomation.h`).
  - **ATL** (`atlbase.h`).
  - **COM** (`comutil.h`).
- **Compiler**: Visual Studio with support for C++17 or later.
- **Privileges**: Administrator privileges are required to run the application.

---

## Installation
1. **Clone the Repository**:

## Usage

The script is very easy to use since it's only a proof of concept. Just execute the script and when using your browser to sign into most websites (though not all) it will copy the username and passwords entered into your browser and will display them in your terminal. 

This isn't the most reliable or effective script but it is enough to show the potential for abuse. Sites confirmed to work are LinkedIn, Okta, and Gmail.
