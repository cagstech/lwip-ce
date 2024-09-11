# Security Policy #

This security policy is designed for lwIP-CE on the TI-84+ CE, a resource-constrained platform. It aims to provide a baseline for secure communication while acknowledging the limitations of the hardware and the nature of the operating environment. Developers and users should be aware of the constraints and the limited ability to handle sensitive or critical information securely.

## 1. Purpose ##

The primary goal of this security policy is to ensure the secure operation of the lwIP-CE network stack while balancing the resource constraints of the TI-84+ CE platform. It provides guidelines for developers and users to minimize vulnerabilities in applications utilizing lwIP-CE.

## 2. Scope ##

This policy applies to all instances of lwIP-CE on the TI-84+ CE and all applications that utilize this network stack for communication over TCP/IP, UDP, ICMP, NTP, DHCP, or other supported protocols. It does not enforce security standards but provides recommendations.

## 3. Security Guidelines for Developers ##

### 3.1 Use Embedded TLS Layer ###

It is the recommendation of the developers of this project that developers using lwIP-CE in their applications do not try to bake their own TLS implementations within their own applications. Use the `altcp_tls` abstraction layer to ensure confidentiality and integrity over the Internet and use the TLS standalone components (hash, key derivation, etc) to offer application-layer security.

### 3.2 Authentication and Encryption ###

  The following algorithms for authentication and encryption are available for use within this library:
  - AES-128-GCM => TLS 1.3 symmetric algorithm
  - Ephemeral ECDH with prime256v1 => TLS 1.3 key negotiation
  - RSA 2048-4096 => TLS 1.3 public key encryption and signature verification
  - Elliptic Curve Digital Signing Algorithm => TLS 1.3 signature verification
  - AES-256-GCM => available, unused in TLS 1.3
  - AES-128-CBC => available, mostly file encryption
  - AES-256-CBC => available, mostly file encryption

### 3.2 Data Integrity and Confidentiality ###

It is strongly recommended not to handle sensitive or personal data on this platform due to the constraints of memory and processing power, which limit the ability to ensure robust security. Developers are responsible for maintaining message integrity through the correct use of use of cryptographic protocols. lwIP-CE does not enforce or verify application-level message integrity.

Due to the slowness of the TI-84+ CE when processing intensive computations such as those relevant to cryptography we have to be aware that if the calculator takes too long to process things, the TLS handshake will time out. For this reason, by default, some of the most intensive portions of client operations, such as certificate chain validation, will be watered down. This may involve only verifying the signature on the leaf certificate, and then verifying issuer back to root without computing signature verification on every other certificate. This is an **implicit trust**. We are assuming that if you are choosing to connect your calculator to a server, you trust it. These security settings will be adjustable--you can disable signature checking entirely or turn it all the way up--but bear in mind that if the calculator sits there for an hour doing all this math, the server will need to be configured to wait that long or things won't work.

### 3.3 Buffer Management and Resource Allocation ###

Strictly manage memory usage to prevent buffer overflows and memory corruption vulnerabilities.
Test applications for correct handling of network traffic, particularly when multiple interfaces (up to 8) are used simultaneously.
Use safe and robust error handling in all network operations to avoid undefined behavior in low-memory conditions.

### 3.5 Auditing and Vulnerability Management ###

lwIP-CE will be periodically updated from the lwIP source and rebuilt, especially if these updates fix critical bugs or security issues. The TLS layer specifically will be updated in response to bug reports tagged *Security* and in response to changes to relevant cryptographic standards. Developers should keep up-to-date with these patches and updates.

lwIP-CE is shared with a community of developers with a good level of expertise on the hardware who assist with algorithms and design. Consistent peer review of code is encouraged to catch potential security issues and we ask that any bugs identified be reported to us promptly.

## 4. User Responsibilities ##

### 4.1 End-User Awareness ###

Users are advised not to use lwIP-CE for critical applications that require high levels of security or that process sensitive data. For example, do not log into your bank account or your store accounts or input your credit card information on your calculator.
Third-party application developers assume all risk for vulnerabilities in their applications resulting from improper use of lwIP-CE.

### 4.2 No Logging or Monitoring ###

Due to resource constraints, lwIP-CE does not support detailed logging or monitoring of network traffic. Users should understand that this limits the ability to detect potential intrusions or unauthorized access. There is no support for intrusion detection or prevention. Application developers must handle security appropriately at the application layer.

## 5. Access Control ##

Applications running on the TI-84+ CE have full control of the device's resources while executing. This means that there is no other kind of code execution exploit, or attack that can occur via a different app or framework, while an application is running. This serves to vastly offset some of the sacrifices made in lwIP-CE for the sake of speed. This means that an application developer that can properly implement security at the application layer and rely on `altcp_tls` for communication can guarantee end-users a modest level of security.

To further promote security of the device and prevent some of the other attacks the calculator may be susceptible to, whenever a cryptographic transformation (such as encryption) is about to begin:

  1. System interrupts are disabled. This prevents USB activity, preventing someone from plugging your calculator into a computer or something else and trying to map out the memory.

When a cryptographic transformation ends, the following actions are taken:

  1. The stack frame is cleared, preventing any residual data from the transformation that might leak data from being recovered after.
  2. System interrupts are returned to their previous state

Additionally, cryptographic transformations are written constant-time to the best extent possible. I wish to impress on you the difficulty in achieving this on a 48 mHz processor. In some cases, we simply cannot get constant-time, but endeavor to ensure that any leakage is only information that would already be known to an attacker.

This means that the app developer is responsible for securing the application and ensuring no unauthorized access occurs during its runtime.
There are no restrictions on internal/external access beyond what the application itself enforces.

## 6. Vulnerability Reporting and Patching ##

Users and developers should report vulnerabilities to the lwIP-CE maintainers for timely patching.
Updates will be provided to fix known vulnerabilities and keep the cryptographic algorithms in line with modern standards.

## 7. Limitations of Security ##

Due to the nature of the TI-84+ CE platform, true security cannot be guaranteed. Resource limitations (memory, processing power) mean that full security measures (e.g., logging, real-time monitoring, advanced cryptographic operations) are not feasible.
Applications that handle sensitive data should consider external review and additional layers of security, especially when TLS is required.

8. Disclaimers

This policy provides best-effort security under the constraints of the platform. Neither lwIP-CE maintainers nor the TI-84+ CE platform developers can be held liable for breaches or vulnerabilities resulting from misuse of the platform.
