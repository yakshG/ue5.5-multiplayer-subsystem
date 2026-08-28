# UE5.5 Multiplayer Subsystem
A `GameInstanceSubsystem` for session management in Unreal Engine 5.5. Supports LAN and Steam (via overlay invites).

## Features
+ Session creation, start, and destruction.
+ LAN session discovery by server name.
+ Steam session joining via overlay invite.
+ Automatic LAN/Steam detection on initialization.
+ Blueprint-assignable delegates for session creation and joining results for easy UI binding.

## Setup
Requirements
+ Unreal Engine 5.5
+ Online Subsystem Steam plugin
+ Steam Sockets plugin

### DefaultEngine.ini
```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SteamSockets.SteamSocketsNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")

[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bRelaunchInSteam=true
bUseSteamNetworking=true
bAllowP2PPacketRelay=true

[SteamSockets]
bEnabled=true

; If using Sessions
bInitServerOnClient=true

[/Script/OnlineSubsystemSteam.SteamNetDriver]
NetConnectionClassName="OnlineSubsystemSteam.SteamNetConnection"
```
>[!NOTE]
The subsystem detects the active Online Subsystem at initialization. If Steam is running and properly configured, it initializes with the Steam subsystem and `IsLAN` is set to `false`. <br>
>If Steam is not running or fails to initialize, the engine falls back to the NULL subsystem and `IsLAN` is set to `true`. The net driver config includes `IpNetDriver` as a fallback, which handles connections via **raw IP and UDP** for standard LAN play.

## Technical Notes
Session discovery worked correctly; sessions were found and returned, but joining through Steam consistently failed silently. The default ```OnlineSubsystemSteam``` configuration did not work as expected for net driver initialization. <br>

Switching to Steam Sockets plugin and updating the net driver to `SteamSocketsNetDriver` did not resolve it on its own. The fix required adding specific session settings during discovery so the client had enough information to resolve the connection; without those settings present in the search result, the join call succeeded internally but the client never traveled.

### Quick Overview
| Mode | Discovery | Join Mechanism |
| --- | --- | --- |
| LAN | Search by server name | Direct join via resolved connection string |
| Steam | Handled via Steam overlay UI | `OnSessionUserInviteAccepted` -> automatic join |

## Built For
[Finding Keys](https://github.com/yakshG/finding-keys-ue5) — a 2-player co-op puzzle game in UE5.5.

## Usage
The subsystem can be accessed from any Blueprint or C++ class.

**Hosting (LAN)**
1. Call `CreateSessionInternal` with a server name.
2. Bind to the `SessionCreateDel` delegate to receive the result.
3. On success, the subsystem starts the session and automatically travels to the game map with `?listen` parameter.

**Joining (LAN)**
1. Call `FindSession` with the host's server name
2. Bind to the `SessionJoinDel` delegate.
3. On success, the client travels to the host's resolved connection string.

**Steam Invites**
1. Create a session with `CreateSessionInternal` with a pre-configured server name.
2. To invite: call `ShowInviteUI` to open the Steam overlay.
3. To join: handled automatically via `OnSessionInviteAccepted`. The subsystem joins the session and travels the client once the invite is accepted in the overlay.

## Notes
+ **Tested** using `SteamDevAppId=480` (Spacewar - Steam's test app)
+ **Limitation**: The invite flow does not currently check if a session already exists on the client before attempting to join a new one.
