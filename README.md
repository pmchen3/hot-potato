# Hot Potato
A game of hot potato.

Start the master program first. Master connects to all players and coordinates the player-to-player connections. Once all connections are established the master passes potato randomly to a player. Then wait to receive potato back and print the trace of the potato.

Players connect to the master and to other players in a ring. They get potato, decrement hops, append their player number to the potato and pass it. When there are no more hops remaining the player passes potato to master to end the game.
