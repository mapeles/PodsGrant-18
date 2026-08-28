#import "PGSProductIDMappingController.h"
#import "PGSProductIDEditingViewController.h"

NSString *_formatProductID(uint16_t product_id) {
	#define RET_PID(name) return [NSString stringWithFormat:NSSTR(name " (%d)"), product_id]
	switch (product_id) {
	case 8219:
		RET_PID("AirPods 4");
	case 8228:
		RET_PID("AirPods Pro 2, USB-C");
	case 0x2014:
		RET_PID("AirPods Pro 2, Lightning");
	case 0x200E:
		RET_PID("AirPods Pro");
	case 8211:
		RET_PID("AirPods 3");
	case 8207:
		RET_PID("AirPods 2");
	case 8214:
		RET_PID("Beats Studio Buds +");
	case 8209:
		RET_PID("Beats Studio Buds");
	default:
		return [NSString stringWithFormat:NSSTR("%d"), product_id];
	}
}

@implementation PGSProductIDMappingController

- (instancetype)initWithConfiguration:(struct podsgrant_settings *)conf {
	_configuration=conf;
	return [super initWithStyle:UITableViewStyleGrouped];
}

- (void)saveSettings {
	if(_configuration->product_id_mapping) {
		for(struct product_id_map_entry_custom *entry=_configuration->product_id_mapping;entry<_configuration->product_id_mapping+_configuration->product_id_mapping_cnt;entry++) {
			if(!entry->original||!entry->target) {
				UIAlertController *invalid_conf_alert=[UIAlertController alertControllerWithTitle:NSSTR("Invalid Configuration") message:NSSTR("You cannot leave a 0 in the configuration.") preferredStyle:UIAlertControllerStyleAlert];
				[invalid_conf_alert addAction:[UIAlertAction actionWithTitle:NSSTR("OK") style:UIAlertActionStyleCancel handler:nil]];
				return;
			}
		}
	}
	if(PGS_saveSettings(_configuration)!=0) {
		UIAlertController *save_error=[UIAlertController alertControllerWithTitle:NSSTR("Save Failed") message:NSSTR("PodsGrant could not save the configuration.") preferredStyle:UIAlertControllerStyleAlert];
		[save_error addAction:[UIAlertAction actionWithTitle:NSSTR("OK") style:UIAlertActionStyleCancel handler:nil]];
		[self presentViewController:save_error animated:YES completion:nil];
		return;
	}
	[self.navigationController popViewControllerAnimated:1];
}

- (void)resetSettings {
	PGS_freeSettings(_configuration);
	PGS_readSettings_to(_configuration, 1);
	[self reloadData];
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gr {
    return NO;
}

- (void)viewWillDisappear:(BOOL)animated {
	[super viewWillDisappear:animated];
	if(self.isMovingFromParentViewController) {
		PGS_freeSettings(_configuration);
		PGS_readSettings_to(_configuration, 1);
	}
}

- (void)viewDidLoad {
	self.title=NSSTR("Product IDs");
	self.navigationItem.rightBarButtonItems=[NSArray arrayWithObjects:[[UIBarButtonItem alloc] initWithTitle:NSSTR("Save") style:UIBarButtonItemStylePlain target:self action:@selector(saveSettings)],[[UIBarButtonItem alloc] initWithTitle:NSSTR("Reset") style:UIBarButtonItemStylePlain target:self action:@selector(resetSettings)],nil];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tv {
	return 2;
}

- (NSInteger)tableView:(id)tv numberOfRowsInSection:(NSInteger)sect {
	if(!sect) {
		return (sizeof(product_id_map_preset)/sizeof(struct product_id_map_entry))-1;
	}
	if(_configuration->product_id_mapping_cnt==255) {
		return 255;	
	}
	return _configuration->product_id_mapping_cnt+1; // + Add button
}

- (NSString *)tableView:(id)tv titleForFooterInSection:(NSInteger)sec {
	if(sec)return nil;
	return NSSTR("You cannot delete those presets, but your configurations would override them.");
}

- (NSString *)tableView:(id)tv titleForHeaderInSection:(NSInteger)section {
	if(!section) {
		return NSSTR("Presets");
	}
	return NSSTR("Customized Configurations");
}

- (void)tableView:(UITableView *)tv didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	[tv deselectRowAtIndexPath:indexPath animated:1];
	if(indexPath.section==0) {
		PGSProductIDEditingViewController *editingVC=[[PGSProductIDEditingViewController alloc] initWithEntry:(struct product_id_map_entry_custom *)(product_id_map_preset+indexPath.row) delegate:nil isConstant:1];
		UINavigationController *nav=[[UINavigationController alloc] initWithRootViewController:editingVC];
		[self presentViewController:nav animated:1 completion:nil];
		return;
	}
	if(indexPath.section&&indexPath.row!=_configuration->product_id_mapping_cnt) {
		PGSProductIDEditingViewController *editingVC=[[PGSProductIDEditingViewController alloc] initWithEntry:(struct product_id_map_entry_custom *)(_configuration->product_id_mapping+indexPath.row) delegate:self isConstant:0];
		UINavigationController *nav=[[UINavigationController alloc] initWithRootViewController:editingVC];
		[self presentViewController:nav animated:1 completion:nil];
		return;
	}else{
			uint8_t new_count=_configuration->product_id_mapping_cnt+1;
			struct product_id_map_entry_custom *resized=realloc(_configuration->product_id_mapping, new_count*sizeof(struct product_id_map_entry_custom));
			if(!resized) {
				UIAlertController *allocation_error=[UIAlertController alertControllerWithTitle:NSSTR("Unable to Add Mapping") message:NSSTR("There is not enough memory to add another mapping.") preferredStyle:UIAlertControllerStyleAlert];
				[allocation_error addAction:[UIAlertAction actionWithTitle:NSSTR("OK") style:UIAlertActionStyleCancel handler:nil]];
				[self presentViewController:allocation_error animated:YES completion:nil];
				return;
			}
			_configuration->product_id_mapping=resized;
			_configuration->product_id_mapping_cnt=new_count;
			struct product_id_map_entry_custom *val=&resized[new_count-1];
			memset(val, 0, sizeof(*val));
		[self reloadData];
		PGSProductIDEditingViewController *editingVC=[[PGSProductIDEditingViewController alloc] initWithEntry:val delegate:self isConstant:0];
		UINavigationController *nav=[[UINavigationController alloc] initWithRootViewController:editingVC];
		[self presentViewController:nav animated:1 completion:nil];
		return;
	}
}

- (void)reloadData {
	[self.tableView reloadData];
}

- (void)deleteConfigurationAtAddress:(struct product_id_map_entry_custom *)addr {
	size_t index=(size_t)(addr-_configuration->product_id_mapping);
	if(index>=_configuration->product_id_mapping_cnt)
		return;
	size_t remaining=_configuration->product_id_mapping_cnt-index-1;
	if(remaining)
		memmove(addr, addr+1, remaining*sizeof(struct product_id_map_entry_custom));
	_configuration->product_id_mapping_cnt--;
	if(!_configuration->product_id_mapping_cnt) {
		free(_configuration->product_id_mapping);
		_configuration->product_id_mapping=NULL;
	}
	[self reloadData];
}

- (UITableViewCell *)tableView:(id)tv cellForRowAtIndexPath:(NSIndexPath *)indexPath {
	if(indexPath.section==0) {
		const struct product_id_map_entry *entry=product_id_map_preset+indexPath.row;
		UITableViewCell *presetCell=[[UITableViewCell alloc] initWithStyle:1 reuseIdentifier:NSSTR("productIDMC_preset")];
		presetCell.textLabel.text=_formatProductID(entry->original);
		presetCell.detailTextLabel.text=_formatProductID(entry->target);
		return presetCell;
	}else if(indexPath.section==1) {
		if(indexPath.row==_configuration->product_id_mapping_cnt) {
			UITableViewCell *add_btn=[UITableViewCell new];
			add_btn.textLabel.text=NSSTR("Add");
			add_btn.textLabel.textColor=[UIColor colorWithRed:0 green:0.478 blue:1 alpha:1];
			return add_btn;
		}
		struct product_id_map_entry_custom *entry=_configuration->product_id_mapping+indexPath.row;
		UITableViewCell *confCell=[[UITableViewCell alloc] initWithStyle:1 reuseIdentifier:NSSTR("productIDMC_conf")];
		confCell.textLabel.text=_formatProductID(entry->original);
		confCell.detailTextLabel.text=_formatProductID(entry->target);
		confCell.accessoryType=UITableViewCellAccessoryDisclosureIndicator;
		return confCell;
	}
	return nil;
}

@end
