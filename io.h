#ifndef IO_H
# define IO_H
#include "db_parse.h"

struct pokemon_t{
  int species_id;
  char name[30];
  char move1[30];
  int move1_priority;
  int move1_accuracy;
  int move1_power;
  char move2[30];
  int move2_priority;
  int move2_accuracy;
  int move2_power;
  int level;
  int hp;
  int current_hp;
  int attack;
  int defense;
  int special_attack;
  int special_defense;
  int speed;
  int gender; // 0 is male    1 is female
};

typedef struct character character_t;
typedef int16_t pair_t[2];

void io_init_terminal(void);
void io_reset_terminal(void);
void io_display(void);
void io_handle_input(pair_t dest);
void io_queue_message(const char *format, ...);
void io_battle(character_t *aggressor, character_t *defender);
void encounter();
void io_display_wild_battle();
void select_pokemon();
void init_pokemon_trainers();
void list_trainers();
pokemon_t create_pokemon();
void deep_copy(pokemon_t dest, pokemon_t source);
void io_display_trainer_battle(character *c);

#endif
