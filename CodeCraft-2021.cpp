#include "head.h"

//可调参数列表

//最大迁出服务器比例
const float MAX_MIGRATE_OUT_SERVER_RATIO = 0.6f;

//购买服务器参数
const float TOTAL_COST_RATIO =0.00055f; 



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

map<C_BoughtServer*, uint32_t, less_BoughtServer<C_BoughtServer*> > DoubleNodeTable;//将所有服务器组织成一个双节点表，键为指向已购买服务器的指针，值为服务器seq
map<C_node*, uint32_t, less_SingleNode<C_node*> > SingleNodeTable;//将所有服务器的节点组织成一个单节点表，键为指向已购买服务器节点的指针，值为服务器seq

unordered_map<uint32_t, S_DeploymentInfo> GlobalVMDeployTable;//全局虚拟机部署表，记录虚拟机id和相应的部署信息
unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;//全局服务器id表，用于从购买序列号到输出id的映射
unordered_map<uint32_t, S_VM> GlobalVMRequestInfo;//全局VMadd请求表，用于从虚拟机id映射到虚拟机信息



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

		while(++right_it != SingleNodeTable.end()){
			if(right_it->first->remaining_cpu_num >= vm.cpu_num && right_it->first->remaining_mem_num >= vm.mem_num){
				break;
			}	
		}
		//如果当前无节点可以容纳此虚拟机
		if(right_it == SingleNodeTable.end()){
			SingleNodeTable.erase(fake_it);
			delete fake_node;
			return false;
		}
		else{
			uint32_t server_seq = right_it->second;
			C_node* node = right_it->first;


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


	map<C_node*, uint32_t, less_SingleNode<C_node*> > tmp_SingleNodeTable(SingleNodeTable);

	//开始遍历当前迁出服务器所有已有的虚拟机
	map<C_node*, uint32_t>::iterator node_out = tmp_SingleNodeTable.end();
	--node_out;

	for(int32_t out = 0;out != max_out; ++out,--node_out){


		//把所有该节点的单节点虚拟机拿出来重新部署

		//拿到当前节点的指针
		C_node *cur_node = node_out->first;
		set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator vm_end = cur_node->single_node_deploy_table.end();
		set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator vm_it = cur_node->single_node_deploy_table.begin(); //指向当前虚拟机

		for(; vm_it != vm_end;){
			const S_VM & vm = *vm_it->vm;
			C_node * fake_node = new C_node(vm);
			//将这个假节点插入到单节点表中，然后以其位置为基准，向右(剩余节点容量升序)寻找最接近的节点
			SingleNodeTable.emplace(fake_node, 0);
			map<C_node*, uint32_t>::iterator fake_it = SingleNodeTable.find(fake_node);
			map<C_node*, uint32_t>::iterator right_it = fake_it;

			while(++right_it != SingleNodeTable.end()){
				if(right_it->first->remaining_cpu_num >= vm.cpu_num && right_it->first->remaining_mem_num >= vm.mem_num){
					if(right_it == SingleNodeTable.find(node_out->first)){
						continue;
					}
					break;
				}	
			}
			//如果当前无节点可以容纳此虚拟机
			if(right_it == SingleNodeTable.end()){
				SingleNodeTable.erase(fake_it);
				delete fake_node;
				vm_it++;
				continue;
			}
			else{
				//从当前节点中删除此虚拟机
				set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator tmp_it = vm_it++;
				//删除前记录此虚拟机的id，用于部署
				uint32_t out_vm_id = tmp_it->vm_id;
				removeVM((tmp_it)->vm_id, node_out->second);


				//新节点部署此虚拟机				
				uint32_t in_server_seq = right_it->second;
				C_node* in_node = right_it->first;
				SingleNodeTable.erase(fake_it);
				delete fake_node; 

				S_MigrationInfo one_migration_info;
				deployVM(out_vm_id, in_server_seq, one_migration_info, in_node);
				day_decision.VM_migrate_vm_record.emplace_back(one_migration_info);

				//迁移数目－1
				--remaining_migrate_vm_num;

				if(remaining_migrate_vm_num == 0)return;
				continue;
			}
			++vm_it;
		}

		set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator double_vm_end = cur_node->double_node_deploy_table.end();
		set<C_VM_Entity, less_VM<C_VM_Entity> >::iterator double_vm_it = cur_node->double_node_deploy_table.begin(); //指向当前虚拟机
		
		for(; double_vm_it != double_vm_end;){
			const S_VM & vm = *(double_vm_it->vm);
			C_BoughtServer * fake_server = new C_BoughtServer(vm);
			//将这个假节点插入到单节点表中，然后以其位置为基准，向右(剩余节点容量升序)寻找最接近的节点
			DoubleNodeTable.emplace(fake_server, 0);
			map<C_BoughtServer*, uint32_t>::iterator fake_it = DoubleNodeTable.find(fake_server);
			map<C_BoughtServer*, uint32_t>::iterator right_it = fake_it;

			while(++right_it != DoubleNodeTable.end()){
			if(right_it->first->A->remaining_cpu_num >= vm.half_cpu_num &&
			right_it->first->B->remaining_cpu_num >= vm.half_cpu_num&&
			right_it->first->A->remaining_mem_num >= vm.half_mem_num&&
			right_it->first->B->remaining_mem_num >= vm.half_mem_num){
				if(right_it == DoubleNodeTable.find(My_servers[node_out->second])){
					continue;
					}
				break;
			}
		}
		if(right_it == DoubleNodeTable.end()){
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


	}
 	#ifdef DO_NODE_BALANCE
 	//对所有服务器做节点均衡
 	assert(remaining_migrate_vm_num > 0);
 	size_t server_size = My_servers.size();
 	S_MigrationInfo migration_info;
 	for(size_t i = 0; i != server_size; ++i){
 		if(My_servers[i].make_node_balance(migration_info, do_balance)){
 			day_decision.VM_migrate_vm_record.emplace_back(migration_info);
 			if(--remaining_migrate_vm_num == 0)return;
 		}
 	}
 	#endif
 }



// void full_loaded_migrate_vm(S_DayTotalDecisionInfo &day_decision,bool do_balance)
// {
// 	int32_t remaining_migrate_vm_num=get_max_migrate_num();
// 	if(remaining_migrate_vm_num<1) return;
	
	
// 	int32_t	singleNodeLength = SingleNodeTable.size();
// 	int32_t max_iter_num = static_cast<int32_t>(My_servers.size() * MAX_MIGRATE_OUT_SERVER_RATIO) * 2;
// 	if(max_iter_num < 1)return;

// 	int32_t count=0;
// 	bool flag=false;

// 	std::mt19937 rng;
//     rng.seed(std::random_device()());
//     std::uniform_int_distribution<std::mt19937::result_type> randLeft(0, singleNodeLength /2 - 1);
// 	std::uniform_int_distribution<std::mt19937::result_type> randRight(singleNodeLength /2, singleNodeLength - 1);


// 	while(++count <= max_iter_num)
// 	{
		
//  		int32_t randomNum_from= randRight(rng);
// 		int32_t randomNum_to=randLeft(rng);
		
// 		map<C_node*,uint32_t>::iterator migrateFrom_it;
// 		map<C_node*,uint32_t>::iterator migrateTo_it;
// 		migrateFrom_it=SingleNodeTable.begin();
// 		advance(migrateFrom_it,randomNum_from);

// 		set<C_VM_Entity>::iterator set_it = migrateFrom_it->first->single_node_deploy_table.begin();
// 		set<C_VM_Entity>::iterator set_end = migrateFrom_it->first->single_node_deploy_table.end();
// 		if(set_it == set_end)continue;

// 		migrateTo_it=SingleNodeTable.begin();
// 		advance(migrateTo_it,randomNum_to);

// 		for(;set_it!= set_end;++set_it)
// 		{
// 			int32_t needCpuNum=set_it->vm->cpu_num;
// 			int32_t needMemNum=set_it->vm->mem_num;

// 			if(migrateTo_it->first->remaining_cpu_num<needCpuNum||migrateTo_it->first->remaining_mem_num<needMemNum) break;

// 			float before = pow(migrateFrom_it->first->remaining_cpu_num+migrateFrom_it->first->remaining_mem_num, 2)+pow(migrateTo_it->first->remaining_cpu_num+migrateTo_it->first->remaining_mem_num, 2);
// 			float after = pow(migrateFrom_it->first->remaining_cpu_num + needCpuNum+migrateFrom_it->first->remaining_mem_num + needMemNum, 2)+pow(migrateTo_it->first->remaining_cpu_num - needCpuNum+migrateTo_it->first->remaining_mem_num - needMemNum, 2);

			
// 			if(before>=after) break;
// 			else{
// 				//迁移操作
// 				S_MigrationInfo one_migration_info;
// 				migrate_vm(set_it->vm_id, migrateTo_it->second,one_migration_info, migrateTo_it->first);
// 				day_decision.VM_migrate_vm_record.emplace_back(one_migration_info);
// 				--remaining_migrate_vm_num;
// 				if(remaining_migrate_vm_num == 0) return;
// 				break;
// 			}
			
// 			//if(abs(after-before)/before<=0.1) flag=true;
			
// 		}
// 		//if(flag) break;
// 	}
// }


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
		for (uint32_t i = 0; i != day_request.request_num; ++i) {
			
			//不断处理请求，直至已有服务器无法满足
			S_DeploymentInfo one_request_deployment_info;
			const S_Request & one_request = day_request.day_request[i];

			//删除虚拟机
			if (!one_request.is_add) {
				assert(GlobalVMDeployTable.find(one_request.vm_id) != GlobalVMDeployTable.end());
				int32_t cur_server_seq = GlobalVMDeployTable[one_request.vm_id].server_seq;
				removeVM(one_request.vm_id, cur_server_seq);
				continue;
			}

			//如果是增加虚拟机
			if (best_fit(one_request, one_request_deployment_info)) {
				day_decision.request_deployment_info.emplace_back(one_request_deployment_info);
				continue;
			};

			const S_VM& vm = VMList[one_request.vm_type];

			//根据所需cpu and mem,购买服务器
			buy_server(vm.cpu_num, vm.mem_num, day_decision.server_bought_kind, t);
			
			best_fit(one_request, one_request_deployment_info);//购买服务器后重新处理当前请求
			day_decision.request_deployment_info.emplace_back(one_request_deployment_info);
		}

		day_decision.Q = day_decision.server_bought_kind.size();
		server_seq_to_id(day_decision);
		
		#ifdef SUBMIT
		write_standard_output(day_decision);
		#endif

		#ifndef SUBMIT
		cout<<"第"<<t<<"天，共有"<<My_servers.size()<<"台服务器"<<endl;
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
	//int start = clock();
	//int end = 0;
	read_standard_input();
	//end = clock();
	//std::cout<<"read input cost: ";
	//std::cout<< (double)(end - start) / CLOCKS_PER_SEC << endl;
	process();
	//start = clock();
	//std::cout<<"process cost: ";
	//std::cout<< (double)(start - end) / CLOCKS_PER_SEC <<endl;
	
	return 0;
}
