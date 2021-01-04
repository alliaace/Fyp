#include "common.h"
#ifdef DEBUG_MODE9
extern FILE* file_pointer_garbage_collection;
#endif
unsigned int G_C_COUNT = 0;

// TEMP
int fail_count = 0;
extern double utilization_ssd;

void G_C_CHECK(void)
{
	int x;
	int i;

	if(total_empty_block_nb <= FLASH_NB * PLANES_PER_FLASH)
	{
		for(i=0; i<GC_VICTIM_NB; i++){
			x = G_C();
			if(x == FAIL){
				break;
			}
		}
	}
}

int G_C(void)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif

	int32_t victim_page_number = VICTIM_BLOCK_SELECTION();
	int page_number_copy;
	int32_t invalid_block_number;

	if(victim_page_number == -1){
//		printf("[%s] There is no victim block \n", __FUNCTION__);
		return FAIL;
	}

	page_number_copy = BM_G_C(victim_page_number);

#ifdef DEBUG_MODE9
	file_pointer_garbage_collection = fopen("./data/p_dbg_9_gc.txt", "a");
	fprintf(file_pointer_garbage_collection,"%d\t%d\t%d\t%lf\t%ld\n", G_C_COUNT, victim_page_number, page_number_copy, ((double)page_number_copy/PAGE_NB)*100, total_empty_block_nb); 	
	fclose(file_pointer_garbage_collection);
#endif
	G_C_COUNT++;

#ifdef MONITOR_ON
	char sizeTemp[1024];
	sprintf(sizeTemp, "GC ");
	WRITE_LOG(sizeTemp);
	sprintf(sizeTemp, "WB AMP %d", page_number_copy);
	WRITE_LOG(sizeTemp);
#endif


#ifdef FTL_DEBUG
	printf("[%s] Complete, total empty block: %ld, %d\n", __FUNCTION__, total_empty_block_nb, page_number_copy);
#endif
	return SUCCESS;
}

int COUNT_INVALID_PAGES(block_state_entry* b_s_entry)
{
	int count=0;
	int i;
	char* valid_array = b_s_entry->valid_array;

        for(i=0;i<PAGE_NB;i++){
                if(valid_array[i] == 'I'){
                        count++;
                }
	}

	return count;
}

int BM_G_C(int32_t victim_page_number)
{
#ifdef FTL_DEBUG
	printf("[%s] Start\n", __FUNCTION__);
#endif
	int32_t new_pbn;
	int32_t v_pbn;
	int valid_page_nb;
	int page_number_copy=0;
	block_state_entry* temp_b_s_entry = GET_BLOCK_STATE_ENTRY(victim_page_number);

	/* If the victim block is not a replacement block */
	if(temp_b_s_entry->rp_root_pbn == -1){
		v_pbn = victim_page_number;
	}
	/* If the victim block is a replacement block */
	else{
		v_pbn = temp_b_s_entry->rp_root_pbn;
	}

	block_state_entry* rp_b_s_entry;
	block_state_entry* root_b_s_entry = GET_BLOCK_STATE_ENTRY(v_pbn);
	rp_block_entry* rp_b_entry = root_b_s_entry->rp_head;
	int n_rp_blocks = root_b_s_entry->rp_count;

	/* Get logical block number of the victim Block */
	int32_t invalid_block_number = GET_INVERSE_MAPPING_INFO(v_pbn);
	if(invalid_block_number == -1){
		printf("ERROR[%s] It is a replacement block !\n", __FUNCTION__);
		return -1;
	}

	/* If the victim block has no replacement block, */
	if(n_rp_blocks == 0){

		/* Get new empty block */
		GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_pbn);

		/* Copy the valid pages in the victim block to the new empty block */
		page_number_copy = VALID_PAGES_COPY(v_pbn, new_pbn);
		SSD_BLOCK_ERASE(CALC_FLASH(v_pbn), CALC_BLOCK(v_pbn));

		/* Update block mapping information */
		UPDATE_OLD_BLOCK_MAPPING(invalid_block_number);
		UPDATE_NEW_BLOCK_MAPPING(invalid_block_number, new_pbn);

		UPDATE_BLOCK_STATE(v_pbn, EMPTY_BLOCK);
		INSERT_EMPTY_BLOCK(v_pbn);
	}
	else{
		/* Get the last replacement block */
		if(n_rp_blocks == 1){
			rp_b_entry = root_b_s_entry->rp_head;
		}
		else if(n_rp_blocks == 2){
			rp_b_entry = root_b_s_entry->rp_head->next;
		}

		/* Get the valid page number of last(tail) replacement block */
		rp_b_s_entry = GET_BLOCK_STATE_ENTRY(rp_b_entry->pbn);
		valid_page_nb = rp_b_s_entry->valid_page_nb;
		int n_invalid_pages = COUNT_INVALID_PAGES(rp_b_s_entry);

		/* If all pages in the block is valid, then */
		if(valid_page_nb == PAGE_NB || n_invalid_pages == 0){

			if(valid_page_nb != PAGE_NB){
				int temp_page_number_copy;
				rp_block_entry* target_rp_b_entry;
				rp_block_entry* first_rp_b_entry;
			
				if(n_rp_blocks == 1){
					target_rp_b_entry = root_b_s_entry->rp_head;
					page_number_copy = VALID_PAGES_COPY(v_pbn, target_rp_b_entry->pbn);
				}
				else if(n_rp_blocks == 2){
					target_rp_b_entry = root_b_s_entry->rp_head->next;
					first_rp_b_entry = root_b_s_entry->rp_head;
					temp_page_number_copy = VALID_PAGES_COPY(v_pbn, target_rp_b_entry->pbn);
					page_number_copy = VALID_PAGES_COPY(first_rp_b_entry->pbn, target_rp_b_entry->pbn);
					page_number_copy += temp_page_number_copy;
				}
			}

			/* Mapping Information Update Start */
                	new_pbn = rp_b_entry->pbn;
			
			/* Update the last rp entry of the last replacement block */
			rp_b_s_entry->rp_root_pbn = -1;

			/* free the last rp entry entry */
			free(rp_b_entry);			

			if(n_rp_blocks == 2){
				/* Update the root entry of the first replacement block */
				temp_b_s_entry = GET_BLOCK_STATE_ENTRY(root_b_s_entry->rp_head->pbn);
				temp_b_s_entry->rp_root_pbn = -1;

				/* Get rid of first rp block */
				SSD_BLOCK_ERASE(CALC_FLASH(root_b_s_entry->rp_head->pbn), CALC_BLOCK(root_b_s_entry->rp_head->pbn));
				UPDATE_BLOCK_STATE(root_b_s_entry->rp_head->pbn, EMPTY_BLOCK);
				INSERT_EMPTY_BLOCK(root_b_s_entry->rp_head->pbn);

				/* free first rp block entry */
				free(root_b_s_entry->rp_head);			
			}

                        /* Update rp block table */
                        root_b_s_entry->rp_count = 0;
                        root_b_s_entry->rp_head = NULL;

                        /* Get rid of original block */
                        SSD_BLOCK_ERASE(CALC_FLASH(v_pbn), CALC_BLOCK(v_pbn));
                        UPDATE_INVERSE_MAPPING(v_pbn, -1);
			UPDATE_BLOCK_STATE(v_pbn, EMPTY_BLOCK);
                        INSERT_EMPTY_BLOCK(v_pbn);

                        /* Update mapping metadata */
                        UPDATE_NEW_BLOCK_MAPPING(invalid_block_number, new_pbn);
		}
                else{
			/* Get new empty block */
                        GET_NEW_BLOCK(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_pbn);

			/* Copy the valid pages to new empty block */
                        page_number_copy = REPLACED_BLOCK_MERGE(v_pbn, new_pbn);

			/* Update Block Mapping Information */
                        UPDATE_OLD_BLOCK_MAPPING(invalid_block_number);
                        UPDATE_NEW_BLOCK_MAPPING(invalid_block_number, new_pbn);
                }
	}

#ifdef FTL_DEBUG
	printf("[%s] End\n", __FUNCTION__);
#endif
	return page_number_copy;
}

/* Greedy Garbage Collection Algorithm */
int32_t VICTIM_BLOCK_SELECTION(void)
{
	int i;
	int n_pb_blocks;
	int32_t lpn;
	int32_t pbn;
	int32_t curr_pbn = -1;
	block_state_entry* curr_b_s_entry;
	int curr_valid_page_nb = PAGE_NB;

	for(i=0;i<BLOCK_MAPPING_ENTRY_NB;i++)
	{
		curr_b_s_entry = GET_BLOCK_STATE_ENTRY(i);
		if(curr_b_s_entry->type == DATA_BLOCK ){

			if(curr_valid_page_nb > curr_b_s_entry->valid_page_nb){
				curr_valid_page_nb = curr_b_s_entry->valid_page_nb;
				curr_pbn = i;
			}
		}

		/* If the block is dead block, quit the loop */
		if(curr_valid_page_nb == 0){
			break;
		}
	}

	if(curr_valid_page_nb == PAGE_NB){
		return -1;
	}

	return curr_pbn;

}

int VALID_PAGES_COPY(int32_t old_pbn, int32_t new_pbn)
{
	int i;
	int page_number_copy = 0;

	unsigned int old_flash_nb = CALC_FLASH(old_pbn);
	unsigned int old_block_nb = CALC_BLOCK(old_pbn);
	unsigned int new_flash_nb = CALC_FLASH(new_pbn);
	unsigned int new_block_nb = CALC_BLOCK(new_pbn);;

	block_state_entry* old_b_s_entry = GET_BLOCK_STATE_ENTRY(old_pbn);
	char* valid_array = old_b_s_entry->valid_array;

	for(i=0;i<PAGE_NB;i++){
		if(valid_array[i] == 'V'){
			SSD_PAGE_READ(old_flash_nb, old_block_nb, i, -1, GC_READ, -1);
			SSD_PAGE_WRITE(new_flash_nb, new_block_nb, i, -1, GC_WRITE, -1);

			UPDATE_BLOCK_STATE_ENTRY(new_pbn, i, VALID);
			UPDATE_BLOCK_STATE_ENTRY(old_pbn, i, INVALID);
			page_number_copy++;
		}
	}

	return page_number_copy;
}



int REPLACED_BLOCK_MERGE(int32_t old_pbn, int32_t new_pbn)
{
 	int i;
	int32_t rp_pbn;
	block_state_entry* b_s_entry;
	int page_number_copy = 0;
	int total_page_number_copy = 0;

	block_state_entry* temp_b_s_entry;
	block_state_entry* root_b_s_entry = GET_BLOCK_STATE_ENTRY(old_pbn);
	rp_block_entry* rp_b_entry = root_b_s_entry->rp_head;
	int n_rp_blocks = root_b_s_entry->rp_count;

	/* Copy The Valid pages of the replacement blocks to new block */
	for(i=0;i<n_rp_blocks;i++){
		if(rp_b_entry==NULL){
			printf("ERROR[%s] rp_b_entry has NULL pointer. \n", __FUNCTION__);
			return -1;
		}	

		rp_pbn = rp_b_entry->pbn;

		page_number_copy = VALID_PAGES_COPY(rp_pbn, new_pbn);
		total_page_number_copy += page_number_copy;

		/* Update the rp block table */
		temp_b_s_entry = GET_BLOCK_STATE_ENTRY(rp_pbn);
		temp_b_s_entry->rp_root_pbn = -1;

		/* Update Metadata */
		SSD_BLOCK_ERASE(CALC_FLASH(rp_pbn), CALC_BLOCK(rp_pbn));
		temp_b_s_entry->erase_count++;
		UPDATE_BLOCK_STATE(rp_pbn, EMPTY_BLOCK);
		INSERT_EMPTY_BLOCK(rp_pbn);

		rp_b_entry = rp_b_entry->next;	
	}

	/* Updat replacement block table */
	if(n_rp_blocks == 1){
		free(root_b_s_entry->rp_head);
	}
	if(n_rp_blocks == 2){
		free(root_b_s_entry->rp_head->next);
		free(root_b_s_entry->rp_head);
	}
	root_b_s_entry->rp_head = NULL;
	root_b_s_entry->rp_count = 0;

	/* Copy The Valid pages of the original block to new block */
	page_number_copy = VALID_PAGES_COPY(old_pbn, new_pbn);
	SSD_BLOCK_ERASE(CALC_FLASH(old_pbn), CALC_BLOCK(old_pbn));
	total_page_number_copy += page_number_copy;

	/* Update the original block Metadata */
	root_b_s_entry->erase_count++;
	UPDATE_BLOCK_STATE(old_pbn, EMPTY_BLOCK);
	INSERT_EMPTY_BLOCK(old_pbn);

	return total_page_number_copy;
}
