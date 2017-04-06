# BattleOfTanks

Problem Statement
Implementing a Multi-player Battle field, Three-level, Interactive Network Game involving Whiteboard and Live Score Display features. 

Game Implementation
The game will be implemented in an authoritative fashion in which all players will connect to a single server.
The server will update all the players regularly regarding the current state of their peers.
Server will also maintain a live scoreboard which will be accessible to all the clients. 

Game Features 
Multiplayer Shooting Game with individual bullet count and health.
Randomized Maze from pre-determined sets of mazes. 
Time Limit for every round.
Live Score Board.
Three levels with increasing difficulty.
A special kind of exchangeable bullet at highest level of difficulty. 

Levels  Description
Level 1 -  Open Battlefield for all players with unlimited bullets and limited health.
Level 2 -  Random Maze with restricted motion for players.
Level 3 -  Random Maze with restricted motion for players and special bullets to break walls.

Challenges
Synchronization the player actions like bullet firing, movement, player status and score board.  
Handling abrupt disconnection of player.
Scores should be carried throughout all the levels even if the player exits from one level.
Different levels of game should be well optimized acc. to the characteristics of player.
Server client acknowledgement should be maintained at proper time intervals to ensure the server and different clients remain connected for smooth functioning of game.
