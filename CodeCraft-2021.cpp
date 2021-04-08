#include "head.h"

//可调参数列表

//最大迁出服务器比例
const float MAX_MIGRATE_OUT_SERVER_RATIO = 0.6f;

//购买服务器参数
const float TOTAL_COST_RATIO =0.00055f; 

//判断虚拟机是否平衡
const float BALANCE_THRESHOLD = 0.3f;

//全局变量定义
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

int32_t N;//可以采购的服务器类型
int32_t M;//虚拟机类型数量
int32_t T;//总共T天
int32_t K;//先给K天
vector<S_Server> ServerList;//用于存储所有可买的服务器种类信息
unordered_map<string, S_VM> VMList;//用于存储所有虚拟机种类信息
vector<S_DayRequest> Requests;//用于存储用户所有的请求信息
vector<C_BoughtServer*> My_servers;//已购买的服务器列表

map<C_BoughtServer*, uint32_t, less_DoubleNode> DoubleNodeTable;//将所有服务器组织成一个双节点表，键为指向已购买服务器的指针，值为服务器seq
map<C_node*, uint32_t, less_SingleNode> SingleNodeTable;//将所有服务器的节点组织成一个单节点表，键为指向已购买服务器节点的指针，值为服务器seq

unordered_map<uint32_t, S_DeploymentInfo> GlobalVMDeployTable;//全局虚拟机部署表，记录虚拟机id和相应的部署信息
unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;//全局服务器id表，用于从购买序列号到输出id的映射
unordered_map<uint32_t, S_VM> GlobalVMRequestInfo;//全局VMadd请求表，用于从虚拟机id映射到虚拟机信息
set<uint32_t> small_VMS;//小虚拟机


//部署虚拟机用
void deployVM(int vm_id, uint32_t server_seq, S_DeploymentInfo& one_deployment_info,C_node* node) {
		const S_VM &vm = GlobalVMRequestInfo[vm_id];
		C_BoughtServer* s = My_servers[server_seq];
		
		//双节点部署
		if(node == nullptr){
			
			SingleNodeTable.erase(s->A);
			SingleNodeTable.erase(s->B);
			DoubleNodeTable.erase(s);

			//更新节点剩余核数与内存
			s->A->remaining_cpu_num -= vm.half_cpu_num;
			s->A->remaining_mem_num -= vm.half_mem_num;
			s->B->remaining_mem_num -= vm.half_mem_num;
			s->B->remaining_cpu_num -= vm.half_cpu_num;

			//更新节点内虚拟机部署表
			s->A->double_node_deploy_table.emplace(&vm, vm_id);
			s->B->double_node_deploy_table.emplace(&vm, vm_id);
			//更新map
			SingleNodeTable.emplace(s->A, server_seq);
			SingleNodeTable.emplace(s->B, server_seq);
			DoubleNodeTable.emplace(s, server_seq);

		}

		else if(node == s->A){
			one_deployment_info.node_name = "A";

			SingleNodeTable.erase(s->A);
			DoubleNodeTable.erase(s);

			s->A->remaining_cpu_num -= vm.cpu_num;
			s->A->remaining_mem_num -= vm.mem_num;

			s->A->single_node_deploy_table.emplace(&vm, vm_id);

			SingleNodeTable.emplace(s->A, server_seq);
			DoubleNodeTable.emplace(s, server_seq);
		}
		else if(node == s->B){
			one_deployment_info.node_name = "B";

			SingleNodeTable.erase(s->B);
			DoubleNodeTable.erase(s);


			s->B->remaining_mem_num -= vm.mem_num;
			s->B->remaining_cpu_num -= vm.cpu_num;

			s->B->single_node_deploy_table.emplace(&vm, vm_id);

			SingleNodeTable.emplace(s->B, server_seq);
			DoubleNodeTable.emplace(s, server_seq);
		}

		assert(s->A->remaining_cpu_num >= 0);
		assert(s->A->remaining_mem_num >= 0);
		assert(s->B->remaining_cpu_num >= 0);
		assert(s->B->remaining_mem_num >= 0);
		
		assert(s->A->remaining_cpu_num <= s->A->total_cpu_num);
		assert(s->A->remaining_mem_num <= s->A->total_mem_num);
		assert(s->B->remaining_cpu_num <= s->B->total_cpu_num);
		assert(s->B->remaining_mem_num <= s->B->total_mem_num);
		
		one_deployment_info.server = s;
		one_deployment_info.vm_info = &vm;
		one_deployment_info.server_seq = server_seq;
		//更新全局虚拟机部署表
		GlobalVMDeployTable.emplace(vm_id, one_deployment_info);
	}

//迁移虚拟机用部署函数
void deployVM(int vm_id, uint32_t server_seq, S_MigrationInfo& one_migration_info, C_node* node) {
		const S_VM& vm = GlobalVMRequestInfo[vm_id];
		S_DeploymentInfo one_deployment_info;
		C_BoughtServer* s = My_servers[server_seq];

		one_migration_info.is_double_node = vm.is_double_node;
		one_migration_info.server_seq = server_seq;
		one_migration_info.vm_id = vm_id;
		if(node == nullptr){
			SingleNodeTable.erase(s->A);
			SingleNodeTable.erase(s->B);
			DoubleNodeTable.erase(s);

			s->A->remaining_cpu_num -= vm.half_cpu_num;
			s->A->remaining_mem_num -= vm.half_mem_num;
			s->B->remaining_mem_num -= vm.half_mem_num;
			s->B->remaining_cpu_num -= vm.half_cpu_num;
			
			//更新节点内虚拟机部署表
			s->A->double_node_deploy_table.emplace(&vm, vm_id);
			s->B->double_node_deploy_table.emplace(&vm, vm_id);
			
			//更新map

			SingleNodeTable.emplace(pair<C_node*, uint32_t>(s->A, server_seq));
			SingleNodeTable.emplace(pair<C_node*, uint32_t>(s->B, server_seq));
			DoubleNodeTable.emplace(pair<C_BoughtServer *, uint32_t>(s, server_seq));

		}

		else if(node == s->A){
			one_deployment_info.node_name = "A";
			one_migration_info.node_name = "A";

			SingleNodeTable.erase(s->A);
			DoubleNodeTable.erase(s);

			s->A->remaining_cpu_num -= vm.cpu_num;
			s->A->remaining_mem_num -= vm.mem_num;

			s->A->single_node_deploy_table.emplace(&vm, vm_id);

			SingleNodeTable.emplace(s->A, server_seq);
			DoubleNodeTable.emplace(s, server_seq);
		}
		else if(node == s->B){
			one_deployment_info.node_name = "B";
			one_migration_info.node_name = "B";
			
			SingleNodeTable.erase(s->B);
			DoubleNodeTable.erase(s);

			s->B->remaining_mem_num -= vm.mem_num;
			s->B->remaining_cpu_num -= vm.cpu_num;

			s->B->single_node_deploy_table.emplace(&vm, vm_id);

			SingleNodeTable.emplace(s->B, server_seq);
			DoubleNodeTable.emplace(s, server_seq);
		}

		assert(s->A->remaining_cpu_num >= 0);
		assert(s->A->remaining_mem_num >= 0);
		assert(s->B->remaining_cpu_num >= 0);
		assert(s->B->remaining_mem_num >= 0);

		assert(s->A->remaining_cpu_num <= s->A->total_cpu_num);
		assert(s->A->remaining_mem_num <= s->A->total_mem_num);
		assert(s->B->remaining_cpu_num <= s->B->total_cpu_num);
		assert(s->B->remaining_mem_num <= s->B->total_mem_num);

		one_deployment_info.server = s;
		one_deployment_info.vm_info = &vm;
		one_deployment_info.server_seq = server_seq;
		//更新全局虚拟机部署表
		GlobalVMDeployTable.emplace(pair<uint32_t, S_DeploymentInfo>(vm_id, one_deployment_info));
		
	}

//从所在服务器上删除虚拟机
void removeVM(uint32_t vm_id, uint32_t server_seq) {
		const S_DeploymentInfo& deployment_info = GlobalVMDeployTable[vm_id];
		const S_VM *vm_info = deployment_info.vm_info;
		C_BoughtServer * s = My_servers[server_seq];
		if (vm_info->is_double_node) {
			SingleNodeTable.erase(s->A);
			SingleNodeTable.erase(s->B);
			DoubleNodeTable.erase(s);

			s->A->remaining_cpu_num += vm_info->half_cpu_num;
			s->B->remaining_cpu_num += vm_info->half_cpu_num;
			s->A->remaining_mem_num += vm_info->half_mem_num;
			s->B->remaining_mem_num += vm_info->half_mem_num;

			s->A->double_node_deploy_table.erase(C_VM_Entity(vm_info, vm_id));
			s->B->double_node_deploy_table.erase(C_VM_Entity(vm_info, vm_id));

			SingleNodeTable.emplace(s->A, server_seq);
			SingleNodeTable.emplace(s->B, server_seq);
			DoubleNodeTable.emplace(s, server_seq);

		}
		else {
			if (deployment_info.node_name == "A") {
				SingleNodeTable.erase(s->A);
				DoubleNodeTable.erase(s);

				s->A->remaining_cpu_num += vm_info->cpu_num;
				s->A->remaining_mem_num += vm_info->mem_num;

				s->A->single_node_deploy_table.erase(C_VM_Entity(vm_info, vm_id));

				SingleNodeTable.emplace(s->A, server_seq);
				DoubleNodeTable.emplace(s, server_seq);
			}
			else {
				SingleNodeTable.erase(s->B);
				DoubleNodeTable.erase(s);

				s->B->remaining_mem_num += vm_info->mem_num;
				s->B->remaining_cpu_num += vm_info->cpu_num;
				
				s->B->single_node_deploy_table.erase(C_VM_Entity(vm_info, vm_id));

				SingleNodeTable.emplace(s->B, server_seq);
				DoubleNodeTable.emplace(s, server_seq);
			}
		}
		assert(s->A->remaining_cpu_num <= s->server_info.cpu_num / 2);
		assert(s->A->remaining_mem_num <= s->server_info.mem_num / 2);
		assert(s->B->remaining_cpu_num <= s->server_info.cpu_num / 2);
		assert(s->B->remaining_mem_num <= s->server_info.mem_num / 2);

		GlobalVMDeployTable.erase(vm_id);
	}

//综合容量、价格购买
void buy_server(int32_t required_cpu, int32_t required_mem, map<string, vector<uint32_t>>& bought_server_kind, int32_t t) {

		//找到一台服务器
		size_t size = ServerList.size();
		int min_idx = 0;
		float min_dis = MAXFLOAT;
		vector<int> accomadatable_seqs;
		
		//先找到所有可容纳当前请求的服务器型号
		for(size_t i = 0; i != size; ++i){
			if((ServerList[i].cpu_num / 2 >= required_cpu) and (ServerList[i].mem_num / 2 >= required_mem)){
				accomadatable_seqs.emplace_back(i);
			}
		}


		//综合考虑价格和容量差，选择一台服务器
		size = accomadatable_seqs.size();
		for(size_t i = 0; i != size; ++i){
			//容量差距
			float vol_dis = sqrt(ServerList[i].cpu_num - required_cpu) + sqrt(ServerList[i].mem_num - required_mem);
			//服务器成本
			float purchase_cost = (T - t) * ServerList[accomadatable_seqs[i]].maintenance_cost + ServerList[accomadatable_seqs[i]].purchase_cost;
			//float purchase_cost = ((T - t) * ServerList[accomadatable_seqs[i]].maintenance_cost + ServerList[accomadatable_seqs[i]].purchase_cost) / (ServerList[i].mem_num + ServerList[i].cpu_num);

			float total_dis = vol_dis + TOTAL_COST_RATIO * purchase_cost;

			if(total_dis < min_dis){
				min_dis = total_dis;
				min_idx = i;
			}
		}

		const S_Server& server =  ServerList[accomadatable_seqs[min_idx]];
		C_BoughtServer *p_bought_server = new C_BoughtServer(server);

		//更新三个表
		My_servers.emplace_back(p_bought_server);
		SingleNodeTable.emplace(p_bought_server->A, p_bought_server->seq);
		SingleNodeTable.emplace(p_bought_server->B, p_bought_server->seq);
		DoubleNodeTable.emplace(p_bought_server, p_bought_server->seq);
	
		//记录购买了哪种服务器，并更新相应决策记录
		if (bought_server_kind.find(server.type) != bought_server_kind.end()) {
			bought_server_kind[server.type].emplace_back(p_bought_server->seq);
		}
		else {
			vector<uint32_t> bought_server_seq_nums;
			bought_server_seq_nums.emplace_back(p_bought_server->seq);
			bought_server_kind.emplace(pair<string, vector<uint32_t>>(server.type, bought_server_seq_nums));
		}

}


//使用best fit 来部署
inline bool best_fit(const S_Request & request, S_DeploymentInfo & one_deployment_info){
	assert(request.is_add);
	const S_VM& vm = VMList[request.vm_type];
	if(!vm.is_double_node){
		C_node * fake_node = new C_node(vm);

		//将这个假节点插入到单节点表中，然后以其位置为基准，向右(剩余节点容量升序)寻找最接近的节点
		SingleNodeTable.emplace(fake_node, 0);
		map<C_node*, uint32_t>::iterator fake_it = SingleNodeTable.find(fake_node);
		assert(fake_it != SingleNodeTable.end());
		map<C_node*, uint32_t>::iterator right_it = fake_it;
		
		//得到所有可容纳当前vm的节点
		vector<C_node *> accomdatable_nodes;
		while(++right_it != SingleNodeTable.end()){
			if(right_it->first->remaining_cpu_num >= vm.cpu_num && right_it->first->remaining_mem_num >= vm.mem_num){
				accomdatable_nodes.emplace_back(right_it->first);
			}	
		}	
		//sort(accomdatable_nodes.begin(), accomdatable_nodes.end(), com_single_node_balance_rate);

		
		//如果当前无节点可以容纳此虚拟机
		if(accomdatable_nodes.size() == 0){
			SingleNodeTable.erase(fake_it);
			delete fake_node;
			return false;
		}
		else{
			//找一个部署后cpu内存比可以变得更均衡的节点
			float min_dis = MAXFLOAT;
			uint32_t min_idx = 0;
			for(uint32_t i = 0; i != accomdatable_nodes.size(); ++i){

				float cur_div_val = static_cast<float>(accomdatable_nodes[i]->remaining_cpu_num - vm.cpu_num) / (accomdatable_nodes[i]->remaining_mem_num - vm.mem_num);
				float div_dis = abs(cur_div_val - 1.0f);
				float cur_dis =  (accomdatable_nodes[i]->remaining_cpu_num - vm.cpu_num)* div_dis + pow(accomdatable_nodes[i]->remaining_cpu_num - vm.cpu_num, 2) + pow(accomdatable_nodes[i]->remaining_mem_num - vm.mem_num, 2);

				if(cur_dis < min_dis){
					min_dis = cur_dis;
					min_idx = i;
				}
			}
		
			right_it = SingleNodeTable.find(accomdatable_nodes[min_idx]);

			uint32_t server_seq = right_it->second;
			C_node* node = right_it->first;
			C_node* the_other_node = My_servers[right_it->second]->A;
			if(the_other_node == node){
				the_other_node = My_servers[right_it->second]->B;
			}
			if(the_other_node->remaining_cpu_num >= vm.cpu_num && the_other_node->remaining_mem_num >= vm.mem_num){
				int32_t r1_cpu = node->remaining_cpu_num;
				int32_t r1_mem = node->remaining_mem_num;
				int32_t r2_cpu = the_other_node->remaining_cpu_num;
				int32_t r2_mem = the_other_node->remaining_mem_num;
				int32_t to_r1_val = pow(r1_cpu - vm.cpu_num - r2_cpu, 2) + pow(r1_mem - vm.mem_num - r2_mem, 2);
				int32_t to_r2_val = pow(r2_cpu - vm.cpu_num - r1_cpu, 2) + pow(r2_mem - vm.mem_num - r1_mem, 2);
				if(to_r2_val < to_r1_val){
					SingleNodeTable.erase(fake_it);
					delete fake_node;

					deployVM(request.vm_id,server_seq, one_deployment_info, the_other_node);
					return true;
				}
			}

			SingleNodeTable.erase(fake_it);
			delete fake_node;

			deployVM(request.vm_id,server_seq, one_deployment_info, node);

			return true;
		}
	}
	else{
		C_BoughtServer* fake_server = new C_BoughtServer(vm);
		DoubleNodeTable.emplace(fake_server, 0);
		map<C_BoughtServer*, uint32_t>::iterator fake_it = DoubleNodeTable.find(fake_server);
		map<C_BoughtServer*, uint32_t>::iterator right_it = fake_it;
		while(++right_it != DoubleNodeTable.end()){
			if(right_it->first->A->remaining_cpu_num >= vm.half_cpu_num &&
			right_it->first->B->remaining_cpu_num >= vm.half_cpu_num&&
			right_it->first->A->remaining_mem_num >= vm.half_mem_num&&
			right_it->first->B->remaining_mem_num >= vm.half_mem_num){
				break;
			}
		}
		if(right_it == DoubleNodeTable.end()){
			DoubleNodeTable.erase(fake_it);
			delete fake_server;
			return false;
		}
		else{
			uint32_t server_seq = right_it->second;

			DoubleNodeTable.erase(fake_it);
			delete fake_server;

			deployVM(request.vm_id, server_seq, one_deployment_info);
			return true;
		}
	}
}


//  //迁移主流程，只进行服务器间迁移，适配最佳适应算法，将服务器资源利用率拉满
void full_loaded_migrate_vm(S_DayTotalDecisionInfo & day_decision, bool do_balance){
	int32_t remaining_migrate_vm_num = get_max_migrate_num();//当天可用迁移量
	if(remaining_migrate_vm_num == 0) return;

	//根据单节点表对所有节点上的单节点部署虚拟机进行迁移
	int32_t max_out = 2 * static_cast<int32_t>(My_servers.size() * MAX_MIGRATE_OUT_SERVER_RATIO);	//查看的单节点个数
	if(max_out < 2)return;


	map<C_node*, uint32_t, less_SingleNode > tmp_SingleNodeTable(SingleNodeTable);

	//开始遍历当前迁出服务器所有已有的虚拟机
	map<C_node*, uint32_t>::iterator node_out = tmp_SingleNodeTable.end();
	--node_out;

	for(int32_t out = 0;out != max_out; ++out,--node_out){


		//把所有该节点的单节点虚拟机拿出来重新部署

		//拿到当前节点的指针
		C_node *cur_node = node_out->first;

		
		set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator double_vm_end = cur_node->double_node_deploy_table.end();
		set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator double_vm_it = cur_node->double_node_deploy_table.begin(); //指向当前虚拟机
		
		for(; double_vm_it != double_vm_end;){
			const S_VM & vm = *(double_vm_it->vm);
			C_BoughtServer * fake_server = new C_BoughtServer(vm);
			//将这个假节点插入到单节点表中，然后以其位置为基准，向右(剩余节点容量升序)寻找最接近的节点
			DoubleNodeTable.emplace(fake_server, 0);
			map<C_BoughtServer*, uint32_t>::iterator fake_it = DoubleNodeTable.find(fake_server);
			map<C_BoughtServer*, uint32_t>::iterator right_it = fake_it;
			bool is_self = false;
			while(++right_it != DoubleNodeTable.end()){
			if(right_it->first->A->remaining_cpu_num >= vm.half_cpu_num &&
			right_it->first->B->remaining_cpu_num >= vm.half_cpu_num&&
			right_it->first->A->remaining_mem_num >= vm.half_mem_num&&
			right_it->first->B->remaining_mem_num >= vm.half_mem_num){
				if(right_it == DoubleNodeTable.find(My_servers[node_out->second])){
					is_self = true;
					break;
					}
				break;
			}
		}
		if(right_it == DoubleNodeTable.end() or is_self){
			DoubleNodeTable.erase(fake_it);
			delete fake_server;
			++double_vm_it;
			continue;
		}
			else{
				//从当前节点中删除此虚拟机
				set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator tmp_it = double_vm_it++;
				//删除前记录此虚拟机的id，用于部署
				uint32_t out_vm_id = tmp_it->vm_id;
				removeVM((tmp_it)->vm_id, node_out->second);


				//新节点部署此虚拟机				
				uint32_t in_server_seq = right_it->second;
				DoubleNodeTable.erase(fake_it);
				delete fake_server; 

				S_MigrationInfo one_migration_info;
				deployVM(out_vm_id, in_server_seq, one_migration_info);
				day_decision.VM_migrate_vm_record.emplace_back(one_migration_info);

				//迁移数目－1
				--remaining_migrate_vm_num;

				if(remaining_migrate_vm_num == 0)return;
				continue;
			}
			++double_vm_it;
		}



		set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator vm_end = cur_node->single_node_deploy_table.end();
		set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator vm_it = cur_node->single_node_deploy_table.begin(); //指向当前虚拟机

		for(; vm_it != vm_end;){
			const S_VM & vm = *vm_it->vm;
			C_node * fake_node = new C_node(vm);
			//将这个假节点插入到单节点表中，然后以其位置为基准，向右(剩余节点容量升序)寻找最接近的节点
			SingleNodeTable.emplace(fake_node, 0);
			map<C_node*, uint32_t>::iterator fake_it = SingleNodeTable.find(fake_node);
			map<C_node*, uint32_t>::iterator right_it = fake_it;
			bool is_self = false;
			while(++right_it != SingleNodeTable.end()){
				if(right_it->first->remaining_cpu_num >= vm.cpu_num && right_it->first->remaining_mem_num >= vm.mem_num){
					if(right_it == SingleNodeTable.find(node_out->first)){
						is_self = true;
						break;
					}
					break;
				}	
			}
			SingleNodeTable.erase(fake_it);
			delete fake_node;
			//如果当前无节点可以容纳此虚拟机
			if(right_it == SingleNodeTable.end() or is_self){
				vm_it++;
				continue;
			}
			else{
				
				//新节点部署此虚拟机				
				uint32_t in_server_seq = right_it->second;
				S_MigrationInfo one_migration_info;

				C_node* in_node = right_it->first;
				C_node* the_other_node = My_servers[right_it->second]->A;
				if(the_other_node == in_node){
				the_other_node = My_servers[right_it->second]->B;
				}
				if(the_other_node->remaining_cpu_num >= vm.cpu_num && the_other_node->remaining_mem_num >= vm.mem_num){
					int32_t r1_cpu = in_node->remaining_cpu_num;
					int32_t r1_mem = in_node->remaining_mem_num;
					int32_t r2_cpu = the_other_node->remaining_cpu_num;
					int32_t r2_mem = the_other_node->remaining_mem_num;
					int32_t to_r1_val = pow(r1_cpu - vm.cpu_num - r2_cpu, 2) + pow(r1_mem - vm.mem_num - r2_mem, 2);
					int32_t to_r2_val = pow(r2_cpu - vm.cpu_num - r1_cpu, 2) + pow(r2_mem - vm.mem_num - r1_mem, 2);
					if(to_r2_val < to_r1_val){
						if(the_other_node == node_out->first){
							vm_it++;
							continue;
						}

						//从当前节点中删除此虚拟机
						set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator tmp_it = vm_it++;
						//删除前记录此虚拟机的id，用于部署
						uint32_t out_vm_id = tmp_it->vm_id;
						removeVM((tmp_it)->vm_id, node_out->second);
						
						deployVM(out_vm_id, in_server_seq, one_migration_info, the_other_node);
						day_decision.VM_migrate_vm_record.emplace_back(one_migration_info);
						
						//迁移数目－1
						--remaining_migrate_vm_num;

						if(remaining_migrate_vm_num == 0)return;
						continue;
					}
				}

				//从当前节点中删除此虚拟机
				set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator tmp_it = vm_it++;
				//删除前记录此虚拟机的id，用于部署
				uint32_t out_vm_id = tmp_it->vm_id;
				removeVM((tmp_it)->vm_id, node_out->second);

				deployVM(out_vm_id, in_server_seq, one_migration_info, in_node);
				day_decision.VM_migrate_vm_record.emplace_back(one_migration_info);

				//迁移数目－1
				--remaining_migrate_vm_num;

				if(remaining_migrate_vm_num == 0)return;
				continue;
			}
			++vm_it;
		}

	}

 }

//主流程
void process() {
	for(int32_t t = 0; t != T; ++t){
		S_DayTotalDecisionInfo day_decision;

		const S_DayRequest& day_request = Requests[t];
		#ifdef MIGRATE
		//根据当天的请求是单节点多还是双节点多来判断是要做节点均衡还是不均衡
		int32_t double_node_add_num = 0;
		int32_t add_request_num = 0;
		bool do_balance = false;

		#ifdef DO_NODE_BALANCE
		for(uint32_t i = 0; i != day_request.request_num; ++i){
			if(!day_request.day_request[i].is_add)continue;
			double_node_add_num = VMList[day_request.day_request[i].vm_type].is_double_node ? double_node_add_num + 1 : double_node_add_num;
			++add_request_num;
		}
		do_balance = double_node_add_num > add_request_num / 2 ? true :false;
		#endif

		//对存量虚拟机进行迁移
		full_loaded_migrate_vm(day_decision, do_balance);
		day_decision.W = day_decision.VM_migrate_vm_record.size();
		#endif

		if(day_request.delete_op_idxs.size() < 2){
			map<const S_Request*, uint32_t, less_Request<const S_Request*> >tmp_request_list;
			for(int i = 0; i != day_request.request_num; ++i){
				tmp_request_list.emplace(&day_request.day_request[i], i);
			}
			map<const S_Request*, uint32_t>::iterator it = tmp_request_list.begin();
			map<const S_Request*, uint32_t>::iterator end = tmp_request_list.end();
			
			map<uint32_t, S_DeploymentInfo> batch_deployment_info;//按序记录当前batch的部署信息
			for(;it != end; ++it){
				//不断处理请求，直至已有服务器无法满足
				S_DeploymentInfo one_request_deployment_info;
				const S_Request & one_request = *(it->first);
				if (best_fit(one_request, one_request_deployment_info)) {
					batch_deployment_info.emplace(it->second, one_request_deployment_info);
					continue;
				};

				const S_VM& vm = VMList[one_request.vm_type];

				//根据所需cpu and mem,购买服务器
				buy_server(vm.cpu_num, vm.mem_num, day_decision.server_bought_kind, t);
				
				best_fit(one_request, one_request_deployment_info);//购买服务器后重新处理当前请求
				batch_deployment_info.emplace(it->second, one_request_deployment_info);
			}

			//将当前批次的部署信息按顺序添加到部署记录中
			map<uint32_t, S_DeploymentInfo>::iterator batch_deploy_it = batch_deployment_info.begin();
			map<uint32_t, S_DeploymentInfo>::iterator batch_deploy_end = batch_deployment_info.end(); 
			for(; batch_deploy_it != batch_deploy_end; ++batch_deploy_it){
				day_decision.request_deployment_info.emplace_back(batch_deploy_it->second);
			}
		
		}
		else{
			int32_t left = 0;
			int32_t right = day_request.delete_op_idxs[0];

			for(uint32_t batch_num = 0;; ){
				map<const S_Request*, uint32_t, less_Request<const S_Request*> >tmp_request_list;
				for(int i = left; i != right; ++i){
					tmp_request_list.emplace(&day_request.day_request[i], i);
				}
				map<const S_Request*, uint32_t>::iterator it = tmp_request_list.begin();
				map<const S_Request*, uint32_t>::iterator end = tmp_request_list.end();
				
				map<uint32_t, S_DeploymentInfo> batch_deployment_info;//按序记录当前batch的部署信息
				for(;it != end; ++it){
					//不断处理请求，直至已有服务器无法满足
					S_DeploymentInfo one_request_deployment_info;
					const S_Request & one_request = *(it->first);
					//std::cout<<"batch_num:"<<batch_num<<"  vm_type:"<<one_request.vm_type<< "is double node:"<<VMList[one_request.vm_type].is_double_node << " vm_cpu:"<< VMList[one_request.vm_type].cpu_num<< " vm_mem:"<<VMList[one_request.vm_type].mem_num<<"  vm_id:"<<one_request.vm_id<<endl;
					//如果是增加虚拟机
					if (best_fit(one_request, one_request_deployment_info)) {
						batch_deployment_info.emplace(it->second, one_request_deployment_info);
						continue;
					};

					const S_VM& vm = VMList[one_request.vm_type];

					//根据所需cpu and mem,购买服务器
					buy_server(vm.cpu_num, vm.mem_num, day_decision.server_bought_kind, t);
					
					best_fit(one_request, one_request_deployment_info);//购买服务器后重新处理当前请求
					batch_deployment_info.emplace(it->second, one_request_deployment_info);
				}
				
				if(batch_num != day_request.delete_op_idxs.size() - 1){
					//处理一个删除请求
					const S_Request & one_request = day_request.day_request.at(day_request.delete_op_idxs[batch_num]);
					
					assert(!one_request.is_add);
					assert(GlobalVMDeployTable.find(one_request.vm_id) != GlobalVMDeployTable.end());
					int32_t cur_server_seq = GlobalVMDeployTable[one_request.vm_id].server_seq;
					removeVM(one_request.vm_id, cur_server_seq);
				}
					
				//将当前批次的部署信息按顺序添加到部署记录中
				map<uint32_t, S_DeploymentInfo>::iterator batch_deploy_it = batch_deployment_info.begin();
				map<uint32_t, S_DeploymentInfo>::iterator batch_deploy_end = batch_deployment_info.end(); 
				for(; batch_deploy_it != batch_deploy_end; ++batch_deploy_it){
					day_decision.request_deployment_info.emplace_back(batch_deploy_it->second);
				}
				if(right == day_request.delete_op_idxs.back()){
					break;
				}

				++batch_num;
				left = right + 1;
				right = day_request.delete_op_idxs[batch_num];
			}
		}
		day_decision.Q = day_decision.server_bought_kind.size();
		server_seq_to_id(day_decision);
		
		#ifdef SUBMIT
		write_standard_output(day_decision);
		#endif

		#ifndef SUBMIT
		std::cout<<"第"<<t<<"天，共有"<<My_servers.size()<<"台服务器"<<endl;
		#endif
		
		if(t < T - K){
			read_one_request();
		}
	}
	/*
	freopen("bought_server_ids.txt", "w", stdout);
	
	for (int32_t i = 0; i != T; ++i) {
		std::cout << "(purchase, " << day_decision.Q << ")" << endl;
		for (map<string, vector<uint32_t>>::iterator iter = day_decision.server_bought_kind.begin(); iter != day_decision.server_bought_kind.end(); ++iter) {
			std::cout << "(" << iter->first << ", " << iter->second.size() << ")" << endl;
			for (int j = 0; j != iter->second.size(); ++j) {
				std::cout << "server type:" << iter->first << "	" << "server seq:" << iter->second[j] << "   " << "server id:" << GlobalServerSeq2IdMapTable[iter->second[j]] << endl;
			}
		}
	}*/

}



int main()
{	
	read_standard_input();
	process();
	return 0;
}
