#include "pebble.h"
#include "util.h"

#define NUM_MENU_SECTIONS 1
#define NUM_MENU_ICONS 2
#define NUM_FIRST_MENU_ITEMS 13
#define MAX_ITEMS 20
#define ITEM_NAME_LENGTH	20
#define ITEM_CHECKED_INDEX 0
#define ITEM_UNCHECKED_INDEX 1
  
#define STORAGE_ITEMS 20
#define STORAGE_ITEM_COUNT 0

// For text scrolling
#define SCROLL_MENU_ITEM_WAIT_TIMER 1000
#define SCROLL_MENU_ITEM_TIMER 400
#define MENU_CHARS_VISIBLE 16
#define SUBTITLE_CHARS_VISIBLE 22

// Pebble KEY
#define KEY_RESULT        1

// Location API
#define KEY_LIST        2

#define MAX_TEXT_SIZE       128

static const uint32_t INBOUND_SIZE	=	128; // Inbound app message size
static const uint32_t OUTBOUND_SIZE	=	128; // Outbound app message size

static Window *s_main_window;
static MenuLayer *s_menu_layer;
static GBitmap *s_menu_icons[NUM_MENU_ICONS];

// TYPEDEF STRUCT def & methods of a list item -------------

typedef struct ListItem ListItem;
struct ListItem {
  char name[ITEM_NAME_LENGTH];
  char description[ITEM_NAME_LENGTH];
  bool is_checked;
};

ListItem list_items[MAX_ITEMS];
int num_items;

void
add_item(char *name, char *description)
{
  if(num_items < MAX_ITEMS){
    strcpy(list_items[num_items].name, name);
    strcpy(list_items[num_items].description, description);
    list_items[num_items].is_checked = false;
    num_items++;    
  }
}

typedef struct WindowData WindowData;
struct WindowData {
  AppTimer *menu_scroll_timer;
  MenuLayer *menu;
  int   menu_scroll_offset;
  bool  moving_forwards_in_menu;
	bool	scrolling_still_required;
  bool	menu_reloading_to_scroll;
};

char list_text[MAX_TEXT_SIZE];

// END OF ListItem --------------------------------------------

// STORAGE ---------------------------------

void load_data() {
  //num_items = persist_read_int(STORAGE_ITEM_COUNT);
  num_items = persist_exists(STORAGE_ITEM_COUNT) ? persist_read_int(STORAGE_ITEM_COUNT) : 0;

  
  for(int i = 0; i < num_items; i++) 
  {
    if (persist_exists(STORAGE_ITEMS + i)) 
    {
      persist_read_data(STORAGE_ITEMS + i, &list_items[i], sizeof(ListItem));
    }
	}
}

void save_data() {
  persist_write_int(STORAGE_ITEM_COUNT, num_items);
  
  if (num_items > 0) 
  {
    for (int i=0; i < num_items; i++)
    {
      persist_write_data(STORAGE_ITEMS + i, &list_items[i], sizeof(ListItem));
    }
  } 
  
}

void clear_persist() {
  num_items = persist_exists(STORAGE_ITEM_COUNT) ? persist_read_int(STORAGE_ITEM_COUNT) : 0;
  
  if (num_items > 0) 
  {
    for (int i=0; i < num_items; i++)
    {
      persist_delete(STORAGE_ITEMS + i);
    }
  } 

}

// END of STORAGE --------------------------

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            //assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        //assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

// START of Receiver ------------------------

static void received_handler(DictionaryIterator *iter, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "ENTORU NO RECEIVED HANDLER ----");
  clear_persist();
  num_items = 0;
  
  Tuple *result_tuple = dict_find(iter, KEY_RESULT);
  if(result_tuple) {
    // Display result to player
    //ui_show_weapon_selector(false);

    // Remember how many games have been played
    //s_game_counter++;
    int item = result_tuple->value->int32;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "item: %d", item);
      
    strcpy(list_text, dict_find(iter, KEY_LIST)->value->cstring);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "List String: %s", list_text);
    
    //char months[] = "Milk,1 units|Eggs,8 units|Ham,200 oz.|Chesse,6 units";
    char** tokens;
    char** comma_tokens;
  
    tokens = str_split(list_text, '|');
  
    if (tokens)
    {
      int i, j;
      for (i = 0; *(tokens + i); i++)
      {
        comma_tokens = str_split(*(tokens + i), ',');
  
        if (comma_tokens) 
        {
          for (j = 1; *(comma_tokens + j); j++)
          {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Item name: %s and qt: %s",  *(tokens + i), *(comma_tokens + j));
  //          printf("Item: %s, %s\n", *(tokens + i), *(comma_tokens + j));
            add_item(*(tokens + i), *(comma_tokens + j));
            free(*(comma_tokens + j));
          }    
        }
        
        free(comma_tokens);
        free(*(tokens + i));
      }
      printf("\n");
      free(tokens);
    }  
    
    
    // Display for 5 seconds
    //app_timer_register(5000, timer_handler, NULL);

    menu_layer_reload_data(s_menu_layer);
    // Go back to low-power mode
    app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
  }
}

/**
 * Failed to receive message
 */
static void dropped_handler(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Message dropped: %s", (char*)reason);
}


// END of Receiver ------------------------

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return NUM_MENU_SECTIONS;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  switch (section_index) {
    case 0:
      return num_items;
    default:
      return 0;
  }
}

static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // Determine which section we're working with
  switch (section_index) {
    case 0:
      // Draw title text in the section header
      menu_cell_basic_header_draw(ctx, cell_layer, "Select item to check");
      break;
  }
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // Determine which section we're going to draw in
  switch (cell_index->section) {
    case 0:
      // Use the row to specify which item we'll draw
      for (int i = 0; i < num_items; i++) {
        if (cell_index->row == i) {
          if (list_items[i].is_checked) {
            menu_cell_basic_draw(ctx, cell_layer, list_items[i].name, list_items[i].description, s_menu_icons[0]);             
          } else {
            menu_cell_basic_draw(ctx, cell_layer, list_items[i].name, list_items[i].description, s_menu_icons[1]);             
          }
        }
      }     
    break;
  }
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
      
  int row_index = cell_index->row;  
  
  for (int i = 0; i < num_items; i++) {  
    if (row_index == i) {
      bool ischecked = list_items[i].is_checked;
      list_items[i].is_checked = !ischecked;
      // After changing the icon, mark the layer to have it updated
      layer_mark_dirty(menu_layer_get_layer(menu_layer));
    }
  }

}

void get_menu_text(WindowData* window_data, int index, char** text, char** subtext) {
//   MenuItem* menu_item = getMenuItem(window_data, index);
//   *text = menu_item ? menu_item->text : NULL;
//   *subtext = menu_item && menu_item->flags & ITEM_FLAG_TWO_LINER ? 
//                          menu_item->text + strlen(menu_item->text) + 1 : NULL;
//   if(*subtext != NULL && strlen(*subtext) == 0) {
//     *subtext = NULL;
//   }
  
  *text = list_items[index].name;
  APP_LOG(APP_LOG_LEVEL_INFO, "Get menu text" );
  MenuIndex menuIndex = menu_layer_get_selected_index(window_data->menu);
  if(*text && menuIndex.row == index) {
    int len = strlen(*text);
    if(len - MENU_CHARS_VISIBLE - window_data->menu_scroll_offset > 0) {
      *text += window_data->menu_scroll_offset;
      window_data->scrolling_still_required = true;
    }
  }
}

static void scroll_menu_callback(void* data) {
 APP_LOG(APP_LOG_LEVEL_DEBUG, "SCROLL MENU CALLBACK timer");
  WindowData* window_data = (WindowData*)data;
  if(!window_data->menu) {
    return;
  }
  window_data->menu_scroll_timer = NULL;
  window_data->menu_scroll_offset++;
  if(!window_data->scrolling_still_required) {
    return;
  }

    // Redraw the menu with this scroll offset
  MenuIndex menuIndex = menu_layer_get_selected_index(window_data->menu);
  if(menuIndex.row != 0) {
    window_data->menu_reloading_to_scroll = true;
  }
  window_data->scrolling_still_required = false;
  menu_layer_reload_data(window_data->menu);
  window_data->menu_scroll_timer = app_timer_register(SCROLL_MENU_ITEM_TIMER, scroll_menu_callback, 
                                                      window_data);
}

static void initiate_menu_scroll_timer(WindowData* window_data) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initiate Menu Scroll Timer");
  // If there is already a timer then reschedule it, otherwise create one
  bool need_to_create_timer = true;
  window_data->scrolling_still_required = true;
  window_data->menu_scroll_offset = 0;
  window_data->menu_reloading_to_scroll = false;
  if(window_data->menu_scroll_timer) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Rescheduling timer");
    need_to_create_timer = !app_timer_reschedule(window_data->menu_scroll_timer, 
                                                SCROLL_MENU_ITEM_WAIT_TIMER);
  }
  if(need_to_create_timer) {
     APP_LOG(APP_LOG_LEVEL_DEBUG, "Creating timer");
    window_data->menu_scroll_timer = app_timer_register(SCROLL_MENU_ITEM_WAIT_TIMER, 
                                                        scroll_menu_callback, window_data);
  }
}

static void selection_changed_callback(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, 
                                       void *data) {
  WindowData* window_data = (WindowData*)data;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Selection Changed Callback");
  
  if (window_data) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "IS NULL");
 
  } else {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "NOT NULL");
      window_data->menu_scroll_offset = 0;    
  }
  
//   APP_LOG(APP_LOG_LEVEL_DEBUG, "Printing bool: %d\n", new_index.row >= old_index.row);

    
//   APP_LOG(APP_LOG_LEVEL_DEBUG, "Printing bool: %d\n", new_index.row >= old_index.row);

//   window_data->moving_forwards_in_menu = false;

//   if(!window_data->menu_reloading_to_scroll) {
//     initiate_menu_scroll_timer(window_data);
//   } else {
//     window_data->menu_reloading_to_scroll = false;
//   }
}



static void main_window_load(Window *window) {
  // Here we load the bitmap assets
  s_menu_icons[0] = gbitmap_create_with_resource(RESOURCE_ID_MENU_CHECKED_ITEM);
  s_menu_icons[1] = gbitmap_create_with_resource(RESOURCE_ID_MENU_UNCHECKED_ITEM);

  // Now we prepare to initialize the menu layer
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  // Create the menu layer
  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .selection_changed = selection_changed_callback,
    .select_click = menu_select_callback
  });

  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(s_menu_layer, window);

  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
  APP_LOG(APP_LOG_LEVEL_INFO, "Main window loaded.");
}

static void main_window_unload(Window *window) {
  // Destroy the menu layer
  menu_layer_destroy(s_menu_layer);

  // Cleanup the menu icons
  for (int i = 0; i < NUM_MENU_ICONS; i++) {
    gbitmap_destroy(s_menu_icons[i]);
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Main window unloaded.");

}

static void init() {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
  
  // Init app message and message handlers
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Initializing handlers");
  app_message_register_inbox_received(received_handler);
  app_message_register_inbox_dropped(dropped_handler);
  app_message_open(INBOUND_SIZE, OUTBOUND_SIZE);
//  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  load_data();
  add_item("This is a really big text", "6 units");
  add_item("This is an even bigger text", "6 units");
//   add_item("Ham", "6 units");
//   add_item("Krokitos", "2 caixas");
  if (num_items == 0) {
    add_item("User your phone", "to add an item");
  }
  
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "NUM ITEMS: %d",  num_items);
  
//   for (int i = 0; i < num_items; i++) {
//       APP_LOG(APP_LOG_LEVEL_DEBUG, "Item name: %s and desc: %d", list_items[i].name, list_items[i].is_checked);
//   }
  
  init();
  app_event_loop();
  deinit();
  save_data();
}
