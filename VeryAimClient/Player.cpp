#include "Player.h"

Player::Player(bool is_ally, bool is_local, int current_hp) {
	ally = is_ally;
	local = is_local;
	hp = current_hp;
}