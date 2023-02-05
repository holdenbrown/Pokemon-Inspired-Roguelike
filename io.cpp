#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <iostream>
using namespace std;
#include<string.h>

#include "io.h"
#include "poke327.h"

typedef struct io_message {
  /* Will print " --more-- " at end of line when another message follows. *
   * Leave 10 extra spaces for that.                                      */
  char msg[71];
  struct io_message *next;
} io_message_t;

static io_message_t *io_head, *io_tail;

void io_init_terminal(void)
{
  initscr();
  raw();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  start_color();
  init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
}

void io_reset_terminal(void)
{
  endwin();

  while (io_head) {
    io_tail = io_head;
    io_head = io_head->next;
    free(io_tail);
  }
  io_tail = NULL;
}

void io_queue_message(const char *format, ...)
{
  io_message_t *tmp;
  va_list ap;

  if (!(tmp = (io_message_t *) malloc(sizeof (*tmp)))) {
    perror("malloc");
    exit(1);
  }

  tmp->next = NULL;

  va_start(ap, format);

  vsnprintf(tmp->msg, sizeof (tmp->msg), format, ap);

  va_end(ap);

  if (!io_head) {
    io_head = io_tail = tmp;
  } else {
    io_tail->next = tmp;
    io_tail = tmp;
  }
}

static void io_print_message_queue(uint32_t y, uint32_t x)
{
  while (io_head) {
    io_tail = io_head;
    attron(COLOR_PAIR(COLOR_CYAN));
    mvprintw(y, x, "%-80s", io_head->msg);
    attroff(COLOR_PAIR(COLOR_CYAN));
    io_head = io_head->next;
    if (io_head) {
      attron(COLOR_PAIR(COLOR_CYAN));
      mvprintw(y, x + 70, "%10s", " --more-- ");
      attroff(COLOR_PAIR(COLOR_CYAN));
      refresh();
      getch();
    }
    free(io_tail);
  }
  io_tail = NULL;
}

/**************************************************************************
 * Compares trainer distances from the PC according to the rival distance *
 * map.  This gives the approximate distance that the PC must travel to   *
 * get to the trainer (doesn't account for crossing buildings).  This is  *
 * not the distance from the NPC to the PC unless the NPC is a rival.     *
 *                                                                        *
 * Not a bug.                                                             *
 **************************************************************************/
static int compare_trainer_distance(const void *v1, const void *v2)
{
  const character *const *c1 = (const character * const *) v1;
  const character *const *c2 = (const character * const *) v2;

  return (world.rival_dist[(*c1)->pos[dim_y]][(*c1)->pos[dim_x]] -
          world.rival_dist[(*c2)->pos[dim_y]][(*c2)->pos[dim_x]]);
}

static character *io_nearest_visible_trainer()
{
  character **c, *n;
  uint32_t x, y, count;

  c = (character **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  n = c[0];

  free(c);

  return n;
}

void io_display()
{
  uint32_t y, x;
  character *c;

  clear();
  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.cur_map->cmap[y][x]) {
        mvaddch(y + 1, x, world.cur_map->cmap[y][x]->symbol);
      } else {
        switch (world.cur_map->map[y][x]) {
        case ter_boulder:
        case ter_mountain:
          attron(COLOR_PAIR(COLOR_MAGENTA));
          mvaddch(y + 1, x, '%');
          attroff(COLOR_PAIR(COLOR_MAGENTA));
          break;
        case ter_tree:
        case ter_forest:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '^');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_path:
        case ter_exit:
          attron(COLOR_PAIR(COLOR_YELLOW));
          mvaddch(y + 1, x, '#');
          attroff(COLOR_PAIR(COLOR_YELLOW));
          break;
        case ter_mart:
          attron(COLOR_PAIR(COLOR_BLUE));
          mvaddch(y + 1, x, 'M');
          attroff(COLOR_PAIR(COLOR_BLUE));
          break;
        case ter_center:
          attron(COLOR_PAIR(COLOR_RED));
          mvaddch(y + 1, x, 'C');
          attroff(COLOR_PAIR(COLOR_RED));
          break;
        case ter_grass:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, ':');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_clearing:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '.');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          attron(COLOR_PAIR(COLOR_CYAN));
          mvaddch(y + 1, x, '0');
          attroff(COLOR_PAIR(COLOR_CYAN)); 
       }
      }
    }
  }

  mvprintw(23, 1, "PC position is (%2d,%2d) on map %d%cx%d%c.",
           world.pc.pos[dim_x],
           world.pc.pos[dim_y],
           abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_x] - (WORLD_SIZE / 2) >= 0 ? 'E' : 'W',
           abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_y] - (WORLD_SIZE / 2) <= 0 ? 'N' : 'S');
  mvprintw(22, 1, "%d known %s.", world.cur_map->num_trainers,
           world.cur_map->num_trainers > 1 ? "trainers" : "trainer");
  mvprintw(22, 30, "Nearest visible trainer: ");
  if ((c = io_nearest_visible_trainer())) {
    attron(COLOR_PAIR(COLOR_RED));
    mvprintw(22, 55, "%c at %d %c by %d %c.",
             c->symbol,
             abs(c->pos[dim_y] - world.pc.pos[dim_y]),
             ((c->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              'N' : 'S'),
             abs(c->pos[dim_x] - world.pc.pos[dim_x]),
             ((c->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              'W' : 'E'));
    attroff(COLOR_PAIR(COLOR_RED));
  } else {
    attron(COLOR_PAIR(COLOR_BLUE));
    mvprintw(22, 55, "NONE.");
    attroff(COLOR_PAIR(COLOR_BLUE));
  }

  io_print_message_queue(0, 0);

  refresh();
}

uint32_t io_teleport_pc(pair_t dest)
{
  /* Just for fun. And debugging.  Mostly debugging. */

  do {
    dest[dim_x] = rand_range(1, MAP_X - 2);
    dest[dim_y] = rand_range(1, MAP_Y - 2);
  } while (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]                  ||
           move_cost[char_pc][world.cur_map->map[dest[dim_y]]
                                                [dest[dim_x]]] == INT_MAX ||
           world.rival_dist[dest[dim_y]][dest[dim_x]] < 0);

  return 0;
}

static void io_scroll_trainer_list(char (*s)[40], uint32_t count)
{
  uint32_t offset;
  uint32_t i;

  offset = 0;

  while (1) {
    for (i = 0; i < 13; i++) {
      mvprintw(i + 6, 19, " %-40s ", s[i + offset]);
    }
    switch (getch()) {
    case KEY_UP:
      if (offset) {
        offset--;
      }
      break;
    case KEY_DOWN:
      if (offset < (count - 13)) {
        offset++;
      }
      break;
    case 27:
      return;
    }

  }
}

static void io_list_trainers_display(npc **c,
                                     uint32_t count)
{
  uint32_t i;
  char (*s)[40]; /* pointer to array of 40 char */

  s = (char (*)[40]) malloc(count * sizeof (*s));

  mvprintw(3, 19, " %-40s ", "");
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], 40, "You know of %d trainers:", count);
  mvprintw(4, 19, " %-40s ", *s);
  mvprintw(5, 19, " %-40s ", "");

  for (i = 0; i < count; i++) {
    snprintf(s[i], 40, "%16s %c: %2d %s by %2d %s",
             char_type_name[c[i]->ctype],
             c[i]->symbol,
             abs(c[i]->pos[dim_y] - world.pc.pos[dim_y]),
             ((c[i]->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              "North" : "South"),
             abs(c[i]->pos[dim_x] - world.pc.pos[dim_x]),
             ((c[i]->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              "West" : "East"));
    if (count <= 13) {
      /* Handle the non-scrolling case right here. *
       * Scrolling in another function.            */
      mvprintw(i + 6, 19, " %-40s ", s[i]);
    }
  }

  if (count <= 13) {
    mvprintw(count + 6, 19, " %-40s ", "");
    mvprintw(count + 7, 19, " %-40s ", "Hit escape to continue.");
    while (getch() != 27 /* escape */)
      ;
  } else {
    mvprintw(19, 19, " %-40s ", "");
    mvprintw(20, 19, " %-40s ",
             "Arrows to scroll, escape to continue.");
    io_scroll_trainer_list(s, count);
  }

  free(s);
}

static void io_list_trainers()
{
  npc **c;
  uint32_t x, y, count;

  c = (npc **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = (npc *) world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  /* Display it */
  io_list_trainers_display(c, count);
  free(c);

  /* And redraw the map */
  io_display();
}

void io_pokemart()
{
  mvprintw(0, 0, "Welcome to the Pokemart.  Could I interest you in some Pokeballs?");
  refresh();
  getch();
}

void io_pokemon_center()
{
  mvprintw(0, 0, "Welcome to the Pokemon Center.  How can Nurse Joy assist you?");
  refresh();
  getch();
}


uint32_t move_pc_dir(uint32_t input, pair_t dest)
{
  int has_moved = 0;
  dest[dim_y] = world.pc.pos[dim_y];
  dest[dim_x] = world.pc.pos[dim_x];

  switch (input) {
  case 1:
  case 2:
  case 3:
    dest[dim_y]++;
    has_moved++;
    break;
  case 4:
  case 5:
  case 6:
    break;
  case 7:
  case 8:
  case 9:
    has_moved++;
    dest[dim_y]--;
    break;
  }
  switch (input) {
  case 1:
  case 4:
  case 7:
    has_moved++;
    dest[dim_x]--;
    break;
  case 2:
  case 5:
  case 8:
    break;
  case 3:
  case 6:
  case 9:
    has_moved++;
    dest[dim_x]++;
    break;
  case '>':
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_mart) {
      io_pokemart();
    }
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_center) {
      io_pokemon_center();
    }
    break;
  }
  int ran = rand() % 2;
  if(has_moved > 0 && world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] == ter_grass
  && ran == 0){
    io_display_wild_battle();
  }

  if (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) {
    if (dynamic_cast<npc *>(world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) &&
        ((npc *) world.cur_map->cmap[dest[dim_y]][dest[dim_x]])->defeated) {
      // Some kind of greeting here would be nice
      return 1;
    } else if (dynamic_cast<npc *>
               (world.cur_map->cmap[dest[dim_y]][dest[dim_x]])) {
      io_battle(&world.pc, world.cur_map->cmap[dest[dim_y]][dest[dim_x]]);
      // Not actually moving, so set dest back to PC position
      dest[dim_x] = world.pc.pos[dim_x];
      dest[dim_y] = world.pc.pos[dim_y];
    }
  }
  
  if (move_cost[char_pc][world.cur_map->map[dest[dim_y]][dest[dim_x]]] ==
      INT_MAX) {
    return 1;
  }

  return 0;
}


pokemon_t create_pokemon(){
  pokemon_t poke;
  float range;
  int move1_id,move2_id;
  int ran = rand() % 1092;
  int dist = (abs(world.cur_idx[dim_x])-200) + (abs(world.cur_idx[dim_y])-200);
  int species_id = pokemon[ran].species_id;
  poke.species_id = species_id;
  strcpy(poke.name,pokemon[ran].identifier);

  if(dist <= 200){
    range = dist / 2 + 1;
  }else{
    range = dist / 2 + 1;
  }
  poke.level = rand() % ((int)range) + 1;

  for(int i = 0; i < 528239; i++){
    if(pokemon_moves[i].pokemon_id == species_id){
      move1_id = pokemon_moves[i].move_id;
      move2_id = pokemon_moves[i+1].move_id;
      break;
    }
  }
  
  for(int i = 0; i < 845; i++){
    if(move1_id == moves[i].id){
      strcpy(poke.move1,moves[i].identifier);
      poke.move1_priority = moves[i].priority;
      poke.move1_accuracy = moves[i].accuracy;
      poke.move1_power = moves[i].power;
    }
    if(move2_id == moves[i].id){
      strcpy(poke.move2,moves[i].identifier);
      poke.move2_priority = moves[i].priority;
      poke.move2_accuracy = moves[i].accuracy;
      poke.move2_power = moves[i].power;
    }
  }

  for(int i = 0; i < 6553; i++){
    if(pokemon_stats[i].pokemon_id == species_id){
      if(pokemon_stats[i].stat_id == 1){
        poke.hp = pokemon_stats[i].base_stat + rand() % 15;
        poke.current_hp = poke.hp;
      }
      if(pokemon_stats[i].stat_id == 2){
        poke.attack = pokemon_stats[i].base_stat + rand() % 15;
      }
      if(pokemon_stats[i].stat_id == 3){
        poke.defense = pokemon_stats[i].base_stat + rand() % 15;
      }
      if(pokemon_stats[i].stat_id == 4){
        poke.special_attack = pokemon_stats[i].base_stat + rand() % 15;
      }
      if(pokemon_stats[i].stat_id == 5){
        poke.special_defense = pokemon_stats[i].base_stat + rand() % 15;
      }
      if(pokemon_stats[i].stat_id == 6){
        poke.speed = pokemon_stats[i].base_stat + rand() % 15;
      }
    }
  }
  poke.gender = rand() % 2;

  return poke;
}

void io_clear(){
  for(int x = 0; x < MAP_X; x++){
    for(int y = 0; y < MAP_Y+1; y++){
      mvprintw(y, x, " ");
    }
  }
  refresh();
}

char io_pc_select(){
  io_clear();
  char key;
  for(int i = 0; i < 6; i++){
    mvprintw(i,0,"%d - %s",i,world.pokemon_pc[i].name);
  }

  while(key != '0' && key != '1' && key != '2' && key != '3' && key != '4' && key != '5'){
    mvprintw(0,30,"- select your pokemon to fight with -");
    key = getch();
  }
  return key;
}

void io_display_wild_battle(){
  pokemon_t p;
  int quit = 0;
  int damage_pc = 0;
  int damage_npc = 0;
  int turn = 0;
  char key;
  int poke_select;
  int i = rand() % 7;
  p = create_pokemon();
  poke_select = 0;
  while(key != 'q' && quit != 1){
    i = rand() % 7;
    key = getch();

  io_clear();
  mvprintw(0,25,"enter q to quit");
  mvprintw(1,25,"enter 1 to fight");
  mvprintw(2,25,"enter 2 to bag");
  mvprintw(3,25,"enter 3 to run");
  mvprintw(4,25,"4 to switch pokemon");

  mvprintw(0,0,"Wild Pokemon");
  mvprintw(1,0,"pokemon: %s",p.name);
  mvprintw(2,0,"level: %d",p.level);
  mvprintw(3,0,"Move 1: %s",p.move1);
  mvprintw(4,0,"Move 2: %s",p.move2);
  mvprintw(5,0,"HP: %d",p.current_hp);
  mvprintw(6,0,"attack: %d",p.attack);
  mvprintw(7,0,"defense: %d",p.defense);
  mvprintw(8,0,"special-attack: %d",p.special_attack);
  mvprintw(9,0,"special-defense: %d",p.special_defense);
  mvprintw(10,0,"speed: %d",p.speed);
  if(p.gender == 0){
    mvprintw(11,0,"Male");
  }else{
    mvprintw(11,0,"Female");
  }

  mvprintw(0,50,"PC pokemon");
  mvprintw(1,50,"pokemon: %s",world.pokemon_pc[poke_select].name);
  mvprintw(2,50,"level: %d",world.pokemon_pc[poke_select].level);
  mvprintw(3,50,"Move 1: %s",world.pokemon_pc[poke_select].move1);
  mvprintw(4,50,"Move 2: %s",world.pokemon_pc[poke_select].move2);
  mvprintw(5,50,"HP: %d",world.pokemon_pc[poke_select].current_hp);
  mvprintw(6,50,"attack: %d",world.pokemon_pc[poke_select].attack);
  mvprintw(7,50,"defense: %d",world.pokemon_pc[poke_select].defense);
  mvprintw(8,50,"special-attack: %d",world.pokemon_pc[poke_select].special_attack);
  mvprintw(9,50,"special-defense: %d",world.pokemon_pc[poke_select].special_defense);
  mvprintw(10,50,"speed: %d",world.pokemon_pc[poke_select].speed);
  if(world.pokemon_pc[poke_select].gender == 0){
    mvprintw(11,50,"Male");
  }else{
    mvprintw(11,50,"Female");
  }

  char key_move = 'j';
  char key_bag = 'j';
  if(key == '1' && world.pokemon_pc[poke_select].current_hp > 0){
    turn = 1;
    while(key_move != 'a' && key_move != 'b'){ 
      key_move = getch();
      mvprintw(6,25,"choose move a(1) or b(2)");
      refresh();
    }
    if(key_move == 'a'){
      damage_pc = (((2*world.pokemon_pc[poke_select].level)/5)+2*world.pokemon_pc[poke_select].move1_power*(world.pokemon_pc[poke_select].attack/world.pokemon_pc[poke_select].defense))/50;
      damage_pc += 2;
      damage_pc += rand() % 10;
    }
    else if(key_move == 'b'){
      damage_pc = (((2*world.pokemon_pc[poke_select].level)/5)+2*world.pokemon_pc[poke_select].move1_power*(world.pokemon_pc[poke_select].attack/world.pokemon_pc[poke_select].defense))/50;
      damage_pc += 2;
      damage_pc += rand() % 10;
    }
    if((rand() % 100)  < world.pokemon_pc[poke_select].move1_accuracy && key_move == 'a'){
      p.current_hp -= damage_pc;
    }else if((rand() % 100)  < world.pokemon_pc[poke_select].move2_accuracy && key_move == 'b'){
      p.current_hp -= damage_pc;
    }
    if(p.current_hp < 0){
      p.current_hp = 0;
    }
  }
  if(key == '3' && i == 0){
    turn = 1;
    quit = 1;
  }else if(key == '4'){
    turn = 1;
    io_clear();
    while(key_bag != '1' && key_bag != '2' && key_bag != '3' 
    && key_bag != '4' && key_bag != '5' && key_bag != '0'){
      mvprintw(0,0,"0 - %s",world.pokemon_pc[0].name);
      mvprintw(1,0,"1 - %s",world.pokemon_pc[1].name);
      mvprintw(2,0,"2 - %s",world.pokemon_pc[2].name);
      mvprintw(3,0,"3 - %s",world.pokemon_pc[3].name);
      mvprintw(4,0,"4 - %s",world.pokemon_pc[4].name);
      mvprintw(5,0,"5 - %s",world.pokemon_pc[5].name);
      refresh();
      key_bag = getch();
    }
    poke_select = key_bag - '0';
  }
  if(key == '2'){
    turn = 1;
    io_clear();
    while(key_bag != 'a' && key_bag != 'b' && key_bag != 'c'){
      mvprintw(0,0,"a - potion %dx",world.potions);
      mvprintw(1,0,"b - revive %dx",world.revives);
      mvprintw(2,0,"c - pokeball %dx",world.balls);
      refresh();
      key_bag = getch();
      if(key_bag == 'a'){
        if(world.pokemon_pc[poke_select].current_hp + 20 > world.pokemon_pc[poke_select].hp){
          world.pokemon_pc[poke_select].current_hp = world.pokemon_pc[poke_select].hp;
        }else{
          world.pokemon_pc[poke_select].current_hp += 20;
        }
      }
      if(key_bag == 'b' && world.pokemon_pc[poke_select].current_hp <= 0){
        world.pokemon_pc[poke_select].current_hp = world.pokemon_pc[poke_select].hp/2;
      }
    }
  }
  if(turn == 1){
    turn = 0;
    if((rand() % 2 + 1) == 1){
      damage_npc = (((2*p.level)/5)+2*p.move1_power*(p.attack/p.defense))/50;
      damage_npc += 2;
      damage_npc += rand() % 10;
      if((rand() % 100) < p.move1_accuracy){
        world.pokemon_pc[poke_select].current_hp -= damage_npc;
      }

    }else{
      damage_npc = (((2*p.level)/5)+2*p.move2_power*(p.attack/p.defense))/50;
      damage_npc += 2;
      damage_npc += rand() % 10;
      if((rand() % 100) < p.move2_accuracy){
        world.pokemon_pc[poke_select].current_hp -= damage_npc;
      }
    }
    if(world.pokemon_pc[poke_select].current_hp <= 0){
      world.pokemon_pc[poke_select].current_hp = 0;
    }
  }
  }
}

void io_battle(character *aggressor, character *defender)
{
  npc *n = (npc *) ((aggressor == &world.pc) ? defender : aggressor);

  n->defeated = 1;
  if (n->ctype == char_hiker || n->ctype == char_rival) {
    n->mtype = move_wander;
  }

  char key;
  int poke_select;
  poke_select = 0;
  while(key != 'q'){

  io_clear();
  mvprintw(0,30,"enter q to flee");

  mvprintw(0,0,"Wild Pokemon");
  mvprintw(1,0,"pokemon: %s",defender->pokemon_char[0].name);
  mvprintw(2,0,"level: %d",defender->pokemon_char[0].level);
  mvprintw(3,0,"Move 1: %s",defender->pokemon_char[0].move1);
  mvprintw(4,0,"Move 2: %s",defender->pokemon_char[0].move2);
  mvprintw(5,0,"HP: %d",defender->pokemon_char[0].hp);
  mvprintw(6,0,"attack: %d",defender->pokemon_char[0].attack);
  mvprintw(7,0,"defense: %d",defender->pokemon_char[0].defense);
  mvprintw(8,0,"special-attack: %d",defender->pokemon_char[0].special_attack);
  mvprintw(9,0,"special-defense: %d",defender->pokemon_char[0].special_defense);
  mvprintw(10,0,"speed: %d",defender->pokemon_char[0].speed);
  if(defender->pokemon_char[0].gender == 0){
    mvprintw(11,0,"Male");
  }else{
    mvprintw(11,0,"Female");
  }

  mvprintw(0,50,"PC pokemon");
  mvprintw(1,50,"pokemon: %s",world.pokemon_pc[poke_select].name);
  mvprintw(2,50,"level: %d",world.pokemon_pc[poke_select].level);
  mvprintw(3,50,"Move 1: %s",world.pokemon_pc[poke_select].move1);
  mvprintw(4,50,"Move 2: %s",world.pokemon_pc[poke_select].move2);
  mvprintw(5,50,"HP: %d",world.pokemon_pc[poke_select].hp);
  mvprintw(6,50,"attack: %d",world.pokemon_pc[poke_select].attack);
  mvprintw(7,50,"defense: %d",world.pokemon_pc[poke_select].defense);
  mvprintw(8,50,"special-attack: %d",world.pokemon_pc[poke_select].special_attack);
  mvprintw(9,50,"special-defense: %d",world.pokemon_pc[poke_select].special_defense);
  mvprintw(10,50,"speed: %d",world.pokemon_pc[poke_select].speed);
  if(world.pokemon_pc[poke_select].gender == 0){
    mvprintw(11,50,"Male");
  }else{
    mvprintw(11,50,"Female");
  }

  refresh();
    key = getch();
  }

}

void select_pokemon(){
  pokemon_t poke1, poke2, poke3;
  poke1 = create_pokemon();
  poke2 = create_pokemon();
  poke3 = create_pokemon();
  char key;

  while(key != '1' && key != '2' && key != '3'){
    mvprintw(0,0,"select a starter pokemon from the 3 provided");
    mvprintw(1,0,"pokemon 1: %s",poke1.name);
    mvprintw(2,0,"pokemon 2: %s",poke2.name);
    mvprintw(3,0,"pokemon 3: %s",poke3.name);
    refresh();
    key = getch();
  }
  if(key == '1'){
    world.pokemon_pc[0] = poke1;
  }else if(key == '2'){
    world.pokemon_pc[0] = poke2;
  }else{
    world.pokemon_pc[0] = poke3;
  }
}

void list_trainers(){
  // char key = 'y';
  int j = 0;
  for(int x = 0; x < MAP_X; x++)
  {
    for(int y = 0; y < MAP_Y; y++)
    {
      if(world.cur_map->cmap[y][x] && 
      (y != world.pc.pos[dim_y] && x != world.pc.pos[dim_x]))
      {
        mvprintw(y,x,"*");
        for(int i = 0; i < 6; i++){
          mvprintw(i,j,"%s",world.cur_map->cmap[y][x]->pokemon_char[i].name);
        }
        j += 10;
      }
    }
  }
  refresh();
  while(1);
}

void init_pokemon_trainers(){
  int i, ran;

  for(int x = 0; x < MAP_X; x++)
  {
    for(int y = 0; y < MAP_Y; y++)
    {
      if(world.cur_map->cmap[y][x] && 
      (y != world.pc.pos[dim_y] && x != world.pc.pos[dim_x]))
      {
        i = 0;
        while((i < 6 && ran <= 6) || i == 0)
        {
          world.cur_map->cmap[y][x]->pokemon_char[i] = create_pokemon();
          ran = rand() % 10 + 1;
          i++;
        }
      }
    }
  }
}

void io_teleport_world(pair_t dest)
{
  /* mvscanw documentation is unclear about return values.  I believe *
   * that the return value works the same way as scanf, but instead   *
   * of counting on that, we'll initialize x and y to out of bounds   *
   * values and accept their updates only if in range.                */
  int x = INT_MAX, y = INT_MAX;
  
  world.cur_map->cmap[world.pc.pos[dim_y]][world.pc.pos[dim_x]] = NULL;

  echo();
  curs_set(1);
  do {
    mvprintw(0, 0, "Enter x [-200, 200]:           ");
    refresh();
    mvscanw(0, 21, "%d", &x);
  } while (x < -200 || x > 200);
  do {
    mvprintw(0, 0, "Enter y [-200, 200]:          ");
    refresh();
    mvscanw(0, 21, "%d", &y);
  } while (y < -200 || y > 200);

  refresh();
  noecho();
  curs_set(0);

  x += 200;
  y += 200;

  world.cur_idx[dim_x] = x;
  world.cur_idx[dim_y] = y;

  new_map(1);
  io_teleport_pc(dest);
}

void io_handle_input(pair_t dest)
{
  uint32_t turn_not_consumed;
  int key;

  do {
    switch (key = getch()) {
    case '7':
    case 'y':
    case KEY_HOME:
      turn_not_consumed = move_pc_dir(7, dest);
      break;
    case '8':
    case 'k':
    case KEY_UP:
      turn_not_consumed = move_pc_dir(8, dest);
      break;
    case '9':
    case 'u':
    case KEY_PPAGE:
      turn_not_consumed = move_pc_dir(9, dest);
      break;
    case '6':
    case 'l':
    case KEY_RIGHT:
      turn_not_consumed = move_pc_dir(6, dest);
      break;
    case '3':
    case 'n':
    case KEY_NPAGE:
      turn_not_consumed = move_pc_dir(3, dest);
      break;
    case '2':
    case 'j':
    case KEY_DOWN:
      turn_not_consumed = move_pc_dir(2, dest);
      break;
    case '1':
    case 'b':
    case KEY_END:
      turn_not_consumed = move_pc_dir(1, dest);
      break;
    case '4':
    case 'h':
    case KEY_LEFT:
      turn_not_consumed = move_pc_dir(4, dest);
      break;
    case '5':
    case ' ':
    case '.':
    case KEY_B2:
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    case '>':
      turn_not_consumed = move_pc_dir('>', dest);
      break;
    case 'Q':
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      world.quit = 1;
      turn_not_consumed = 0;
      break;
      break;
    case 't':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
    case 'p':
      /* Teleport the PC to a random place in the map.               */
      io_teleport_pc(dest);
      turn_not_consumed = 0;
      break;
    case 'm':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
    case 'f':
      /* Fly to any map in the world.                                */
      io_teleport_world(dest);
      turn_not_consumed = 0;
      break;
    case 'q':
      /* Demonstrate use of the message queue.  You can use this for *
       * printf()-style debugging (though gdb is probably a better   *
       * option.  Not that it matters, but using this command will   *
       * waste a turn.  Set turn_not_consumed to 1 and you should be *
       * able to figure out why I did it that way.                   */
      io_queue_message("This is the first message.");
      io_queue_message("Since there are multiple messages, "
                       "you will see \"more\" prompts.");
      io_queue_message("You can use any key to advance through messages.");
      io_queue_message("Normal gameplay will not resume until the queue "
                       "is empty.");
      io_queue_message("Long lines will be truncated, not wrapped.");
      io_queue_message("io_queue_message() is variadic and handles "
                       "all printf() conversion specifiers.");
      io_queue_message("Did you see %s?", "what I did there");
      io_queue_message("When the last message is displayed, there will "
                       "be no \"more\" prompt.");
      io_queue_message("Have fun!  And happy printing!");
      io_queue_message("Oh!  And use 'Q' to quit!");

      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    default:
      /* Also not in the spec.  It's not always easy to figure out what *
       * key code corresponds with a given keystroke.  Print out any    *
       * unhandled key here.  Not only does it give a visual error      *
       * indicator, but it also gives an integer value that can be used *
       * for that key in this (or other) switch statements.  Printed in *
       * octal, with the leading zero, because ncurses.h lists codes in *
       * octal, thus allowing us to do reverse lookups.  If a key has a *
       * name defined in the header, you can use the name here, else    *
       * you can directly use the octal value.                          */
      mvprintw(0, 0, "Unbound key: %#o ", key);
      turn_not_consumed = 1;
    }
    refresh();
  } while (turn_not_consumed);
}
