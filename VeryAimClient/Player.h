#pragma once
class Player 
{
	bool ally;
	bool local;
	int hp;

public:
	Player(bool ally, bool local, int hp);

	bool is_ally();
	bool is_local();
	int get_hp();
};

