./station-server.py StationA 6001 6002 localhost:6004 &
./station-server.py TerminalB 6003 6004 localhost:6002 localhost:6006 &
./station-server.py JunctionC 6005 6006 localhost:6004 &
