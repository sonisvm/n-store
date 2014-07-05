// ARIES LOGGING

#include "aries_engine.h"

using namespace std;

aries_engine::aries_engine(const config& _conf)
    : conf(_conf),
      db(conf.db) {

  //for (int i = 0; i < conf.num_executors; i++)
  //  executors.push_back(std::thread(&wal_engine::runner, this));

}

aries_engine::~aries_engine() {

  // done = true;
  //for (int i = 0; i < conf.num_executors; i++)
  //  executors[i].join();

}

std::string aries_engine::select(const statement& st) {
  LOG_INFO("Select");
  record* rec_ptr = st.rec_ptr;
  table* tab = db->tables->at(st.table_id);
  table_index* table_index = tab->indices->at(st.table_index_id);

  unsigned long key = hash_fn(st.key);
  std::string val;

  rec_ptr = table_index->map->at(key);
  val = get_data(rec_ptr, st.projection);
  LOG_INFO("val : %s", val.c_str());

  return val;
}

void aries_engine::insert(const statement& st) {
  LOG_INFO("Insert");
  record* after_rec = st.rec_ptr;
  table* tab = db->tables->at(st.table_id);
  plist<table_index*>* indices = tab->indices;

  unsigned int num_indices = tab->num_indices;
  unsigned int index_itr;

  std::string key_str = get_data(after_rec, indices->at(0)->sptr);
  unsigned long key = hash_fn(key_str);

  // Check if key exists
  if (indices->at(0)->map->contains(key) != 0) {
    return;
  }

  // Add log entry
  entry_stream.str("");
  entry_stream << st.transaction_id << " " << st.op_type << " " << st.table_id
               << " " << get_data(after_rec, after_rec->sptr);
  entry_str = entry_stream.str();
  undo_log.push_back(entry_str);

  // Add entry in indices
  for (index_itr = 0; index_itr < num_indices; index_itr++) {
    key_str = get_data(after_rec, indices->at(index_itr)->sptr);
    key = hash_fn(key_str);

    indices->at(index_itr)->map->insert(key, after_rec);
  }
}

void aries_engine::remove(const statement& st) {
  LOG_INFO("Remove");
  record* rec_ptr = st.rec_ptr;
  table* tab = db->tables->at(st.table_id);
  plist<table_index*>* indices = tab->indices;

  unsigned int num_indices = tab->num_indices;
  unsigned int index_itr;

  std::string key_str = get_data(rec_ptr, indices->at(0)->sptr);
  unsigned long key = hash_fn(key_str);

  // Check if key does not exist
  if (indices->at(0)->map->contains(key) == 0) {
    return;
  }

  record* before_rec = indices->at(0)->map->at(key);

  // Add log entry
  entry_stream.str("");
  entry_stream << st.transaction_id << " " << st.op_type << " " << st.table_id
               << " " << get_data(before_rec, before_rec->sptr);

  entry_str = entry_stream.str();
  undo_log.push_back(entry_str);

  // Remove entry in indices
  for (index_itr = 0; index_itr < num_indices; index_itr++) {
    key_str = get_data(rec_ptr, indices->at(index_itr)->sptr);
    key = hash_fn(key_str);

    indices->at(index_itr)->map->erase(key);
  }

}

void aries_engine::update(const statement& st) {
  LOG_INFO("Update");
  record* rec_ptr = st.rec_ptr;
  plist<table_index*>* indices = db->tables->at(st.table_id)->indices;

  std::string key_str = get_data(rec_ptr, indices->at(0)->sptr);
  unsigned long key = hash_fn(key_str);

  record* before_rec = indices->at(0)->map->at(key);

  // Check if key does not exist
  if (before_rec == 0)
    return;

  void *before_field, *after_field;
  int num_fields = st.field_ids.size();

  entry_stream.str("");
  entry_stream << st.transaction_id << " " << st.op_type << " " << st.table_id
               << " ";
  // before image
  entry_stream << get_data(before_rec, before_rec->sptr) << " ";

  for (int field_itr : st.field_ids) {
    // Pointer field
    if (rec_ptr->sptr->columns[field_itr].inlined == 0) {
      before_field = before_rec->get_pointer(field_itr);
      after_field = rec_ptr->get_pointer(field_itr);
    }
    // Data field
    else {
      before_field = before_rec->get_pointer(field_itr);
      std::string before_data = before_rec->get_data(field_itr);
    }
  }

  for (int field_itr : st.field_ids) {
    // Update existing record
    before_rec->set_data(field_itr, rec_ptr);
  }

  // after image
  entry_stream << get_data(before_rec, before_rec->sptr) << " ";

  // Add log entry
  entry_str = entry_stream.str();
  undo_log.push_back(entry_str);
}

// RUNNER + LOADER

int looper = 0;

void aries_engine::execute(const transaction& txn) {

  for (const statement& st : txn.stmts) {
    if (st.op_type == operation_type::Select) {
      select(st);
    } else if (st.op_type == operation_type::Insert) {
      insert(st);
    } else if (st.op_type == operation_type::Update) {
      update(st);
    } else if (st.op_type == operation_type::Delete) {
      remove(st);
    }
  }

  // Sync log
  if(++looper%4096 ==0){
    undo_log.write();
  }

}

void aries_engine::runner() {
  bool empty = true;

  while (!done) {
    rdlock(&txn_queue_rwlock);
    empty = txn_queue.empty();
    unlock(&txn_queue_rwlock);

    if (!empty) {
      wrlock(&txn_queue_rwlock);
      const transaction& txn = txn_queue.front();
      txn_queue.pop();
      unlock(&txn_queue_rwlock);

      execute(txn);
    }
  }

  while (!txn_queue.empty()) {
    wrlock(&txn_queue_rwlock);
    const transaction& txn = txn_queue.front();
    txn_queue.pop();
    unlock(&txn_queue_rwlock);

    execute(txn);
  }
}

void aries_engine::generator(const workload& load, bool stats) {

  undo_log.configure(conf.fs_path + "log");
  timespec time1, time2;
  clock_gettime(CLOCK_REALTIME, &time1);

  for (const transaction& txn : load.txns)
    execute(txn);

  clock_gettime(CLOCK_REALTIME, &time2);

  if (stats)
    display_stats(time1, time2, load.txns.size());

  undo_log.close();
}

void aries_engine::recovery() {

  /*
   int op_type, txn_id, table_id;
   table *tab;
   plist<table_index*>* indices;
   unsigned int num_indices, index_itr;
   record *before_rec, *after_rec;
   field_info finfo;

   for (char* ptr : undo_vec) {
   LOG_INFO("entry : %s ", ptr);

   int offset = 0;
   offset += std::sscanf(ptr + offset, "%d %d %d ", &txn_id, &op_type,
   &table_id);
   offset += 3;

   switch (op_type) {
   case operation_type::Insert:
   LOG_INFO("Reverting Insert");
   std::sscanf(ptr + offset, "%p", &after_rec);

   tab = db->tables->at(table_id);
   indices = tab->indices;
   num_indices = tab->num_indices;

   // Remove entry in indices
   for (index_itr = 0; index_itr < num_indices; index_itr++) {
   std::string key_str = get_data(after_rec,
   indices->at(index_itr)->sptr);
   unsigned long key = hash_fn(key_str);

   indices->at(index_itr)->map->erase(key);
   }

   break;

   case operation_type::Delete:
   LOG_INFO("Reverting Delete");
   std::sscanf(ptr, "%p ", &before_rec);

   tab = db->tables->at(table_id);
   indices = tab->indices;
   num_indices = tab->num_indices;

   // Fix entry in indices to point to before_rec
   for (index_itr = 0; index_itr < num_indices; index_itr++) {
   std::string key_str = get_data(before_rec,
   indices->at(index_itr)->sptr);
   unsigned long key = hash_fn(key_str);

   indices->at(index_itr)->map->insert(key, before_rec);
   }
   break;

   case operation_type::Update:
   LOG_INFO("Reverting Update");
   int num_fields;
   int field_itr;
   offset += std::sscanf(ptr + offset, "%d ", &num_fields);
   offset += 1;

   for (field_itr = 0; field_itr < num_fields; field_itr++) {

   offset += std::sscanf(ptr + offset, "%d %p", &field_itr, &before_rec);
   offset += 1;

   tab = db->tables->at(table_id);
   indices = tab->indices;
   finfo = before_rec->sptr->columns[field_itr];

   // Pointer
   if (finfo.inlined == 0) {
   LOG_INFO("Pointer ");
   void *before_field, *after_field;

   offset += std::sscanf(ptr + offset, "%p %p", &before_field,
   &after_field);
   before_rec->set_pointer(field_itr, before_field);

   }
   // Data
   else {
   LOG_INFO("Inlined ");

   field_type type = finfo.type;
   size_t field_offset = before_rec->sptr->columns[field_itr].offset;

   switch (type) {
   case field_type::INTEGER:
   int ival;
   offset += std::sscanf(ptr + offset, "%d", &ival);
   offset += 1;
   std::sprintf(&(before_rec->data[field_offset]), "%d", ival);
   break;

   case field_type::DOUBLE:
   double dval;
   offset += std::sscanf(ptr + offset, "%lf", &dval);
   offset += 1;
   std::sprintf(&(before_rec->data[field_offset]), "%lf", dval);
   break;

   default:
   cout << "Invalid field type : " << op_type << endl;
   break;
   }
   }
   }

   break;

   default:
   cout << "Invalid operation type" << op_type << endl;
   break;
   }

   delete ptr;
   }
   */

}
