#import <Foundation/Foundation.h>
#import "PGSRootListController.h"
#import "PGSProductIDMappingController.h"
#import "PGSCreditsViewController.h"
#include <xpc/xpc.h>

#define PGS_HELPER_SERVICE "com.lns.pogr.respring"

extern xpc_connection_t PGSXPCConnectionCreateMachService(const char *name, dispatch_queue_t queue, uint64_t flags)
	__asm("_xpc_connection_create_mach_service");

@implementation PGSRootListController

// For iOS 13.*
- (id)specifier {
	return nil;
}

- (void)setRootController:(id)rc {
}
- (void)setParentController:(id)rc {
}
- (void)setSpecifier:(id)spec {
}

- (instancetype)initWithStyle:(UITableViewStyle)style {
	self=[super initWithStyle:UITableViewStyleGrouped];
	if(self)
		PGS_readSettings_to(&_configuration, 1);
	return self;
}

- (void)viewDidLoad {
	self.title=[NSString stringWithUTF8String:"PodsGrant"];
}

- (NSInteger)tableView:(id)tv numberOfRowsInSection:(NSInteger)section {
	if(!section)
		return 1;
	if(section==1)
		return 1;
	if(section==2)
		return 5;
	return 0;
}

- (NSInteger)numberOfSectionsInTableView:(id)tv {
	return 3;
}

- (void)_sw_enabled_switch:(UISwitch *)the_switch {
	BOOL previousValue=_configuration.is_tweak_enabled;
	_configuration.is_tweak_enabled=the_switch.on;
	if(PGS_saveSettings(&_configuration)!=0) {
		_configuration.is_tweak_enabled=previousValue;
		the_switch.on=previousValue;
		[self showHelperError:-4];
	}
}

- (void)showHelperError:(int64_t)errorCode {
	NSString *message=[NSString stringWithFormat:@"PodsGrant could not complete the action (error %lld).", errorCode];
	UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Action Failed" message:message preferredStyle:UIAlertControllerStyleAlert];
	[alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
	[self presentViewController:alert animated:YES completion:nil];
}

- (void)sendHelperCommand:(const char *)command {
	BOOL connectionWillTerminate=strcmp(command, "respring")==0;
	xpc_connection_t connection=PGSXPCConnectionCreateMachService(PGS_HELPER_SERVICE, dispatch_get_main_queue(), 0);
	if(!connection) {
		[self showHelperError:-1];
		return;
	}
	xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
		if(xpc_get_type(event)==XPC_TYPE_ERROR && !connectionWillTerminate)
			[self showHelperError:-2];
	});
	xpc_connection_resume(connection);
	xpc_object_t request=xpc_dictionary_create(NULL, NULL, 0);
	xpc_dictionary_set_string(request, "command", command);
	xpc_connection_send_message_with_reply(connection, request, dispatch_get_main_queue(), ^(xpc_object_t reply) {
		BOOL isDictionary=xpc_get_type(reply)==XPC_TYPE_DICTIONARY;
		if((isDictionary && !xpc_dictionary_get_bool(reply, "success")) || (!isDictionary && !connectionWillTerminate)) {
			int64_t errorCode=isDictionary ? xpc_dictionary_get_int64(reply, "error") : -3;
			[self showHelperError:errorCode];
		}
		xpc_connection_cancel(connection);
	});
}

- (void)confirmRespring {
	UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Restart SpringBoard?" message:@"This uses Dopamine's jbctl respring path." preferredStyle:UIAlertControllerStyleAlert];
	[alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
	[alert addAction:[UIAlertAction actionWithTitle:@"Restart" style:UIAlertActionStyleDestructive handler:^(UIAlertAction *action) {
		(void)action;
		[self sendHelperCommand:"respring"];
	}]];
	[self presentViewController:alert animated:YES completion:nil];
}

- (void)tableView:(UITableView *)tv didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	if(indexPath.section==2) {
		if(!indexPath.row) {
			[self sendHelperCommand:"reload-daemons"];
			[tv deselectRowAtIndexPath:indexPath animated:YES];
			return;
		}else if(indexPath.row==1) {
			[tv deselectRowAtIndexPath:indexPath animated:YES];
			[self confirmRespring];
			return;
		}else if(indexPath.row==2) {
			[tv deselectRowAtIndexPath:indexPath animated:1];
			if(check_update_in_progress) {
				return;
			}
			NSError *status_file_err=nil;
			NSString *status_file=[NSString stringWithContentsOfFile:@"/var/lib/dpkg/status" encoding:NSUTF8StringEncoding error:&status_file_err];
			if(status_file_err) {
				status_file=[NSString stringWithContentsOfFile:@"/var/jb/var/lib/dpkg/status" encoding:NSUTF8StringEncoding error:&status_file_err];
				if(status_file_err) {
					UIAlertController *failed_alert=[UIAlertController alertControllerWithTitle:@"Failed" message:@"Failed to check for update as dpkg status file isn't present." preferredStyle:UIAlertControllerStyleAlert];
					[failed_alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
					[self presentViewController:failed_alert animated:1 completion:nil];
					return;
				}
			}
			NSRegularExpression *versionInfoRegex=[NSRegularExpression regularExpressionWithPattern:@"Package: com.lns.pogr\n(.|\n)*?Version: (\\d\\.\\d\\.\\d)(-|~|\n)" options:0 error:nil];
			NSTextCheckingResult *_match=[versionInfoRegex firstMatchInString:status_file options:0 range:NSMakeRange(0, [status_file length])];
			if(![_match numberOfRanges]||[_match numberOfRanges]<=2) {
				regex_error_pos:{}
				UIAlertController *failed_alert=[UIAlertController alertControllerWithTitle:@"Failed" message:@"Failed to check for update, this tweak isn't installed property." preferredStyle:UIAlertControllerStyleAlert];
				[failed_alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
				[self presentViewController:failed_alert animated:1 completion:nil];
				return;
			}
			NSString *tweak_version=[status_file substringWithRange:[_match rangeAtIndex:2]];
			NSRegularExpression *preciseVersionRegex=[NSRegularExpression regularExpressionWithPattern:@"(\\d)\\.(\\d)\\.(\\d)" options:0 error:nil];
			NSTextCheckingResult *pv_match=[preciseVersionRegex firstMatchInString:tweak_version options:0 range:NSMakeRange(0, tweak_version.length)];
			if([pv_match numberOfRanges]<=3)
				goto regex_error_pos;
			int tweak_version_major=[tweak_version substringWithRange:[pv_match rangeAtIndex:1]].intValue;
			int tweak_version_minor=[tweak_version substringWithRange:[pv_match rangeAtIndex:2]].intValue;
			int tweak_version_patch=[tweak_version substringWithRange:[pv_match rangeAtIndex:3]].intValue;
			check_update_in_progress=1;
			[tv reloadData];
			dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
				NSURLSession *session=[NSURLSession sharedSession];
				NSURLSessionDataTask *task=[session dataTaskWithURL:[NSURL URLWithString:@"https://api.github.com/repos/mapeles/PodsGrant-18/releases/latest"] completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
					(void)response;
					dispatch_async(dispatch_get_main_queue(), ^{
						if(error) {
							UIAlertController *failed_alert=[UIAlertController alertControllerWithTitle:@"Failed" message:@"Failed to check for update, request failed, check your network connection." preferredStyle:UIAlertControllerStyleAlert];
							[failed_alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
							[self presentViewController:failed_alert animated:1 completion:nil];
							check_update_in_progress=0;
							[tv reloadData];
							return;
						}
						NSDictionary *githubApiData=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
						if(!githubApiData) {
							api_error_pos:{}
							UIAlertController *failed_alert=[UIAlertController alertControllerWithTitle:@"Failed" message:@"Failed to check for update, invalid response received." preferredStyle:UIAlertControllerStyleAlert];
							[failed_alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
							[self presentViewController:failed_alert animated:1 completion:nil];
							check_update_in_progress=0;
							[tv reloadData];
							return;
						}
						NSString *latest_version=githubApiData[@"tag_name"];
						NSTextCheckingResult *lv_match=[preciseVersionRegex firstMatchInString:latest_version options:0 range:NSMakeRange(0, latest_version.length)];
						if([lv_match numberOfRanges]<=3)
							goto api_error_pos;
						int latest_version_major=[latest_version substringWithRange:[lv_match rangeAtIndex:1]].intValue;
						int latest_version_minor=[latest_version substringWithRange:[lv_match rangeAtIndex:2]].intValue;
						int latest_version_patch=[latest_version substringWithRange:[lv_match rangeAtIndex:3]].intValue;
						NSString *update_str=[NSString stringWithFormat:@"Your version (%@) is up to date.", tweak_version];
						if(tweak_version_major<latest_version_major) {
							goto update_found;
						}else if(tweak_version_major==latest_version_major) {
							if(tweak_version_minor<latest_version_minor) {
								goto update_found;
							}else if(tweak_version_minor==latest_version_minor&&tweak_version_patch<latest_version_patch) {
								goto update_found;
							}
						}
						if(0) {
							update_found:
							update_str=[NSString stringWithFormat:@"An updated version (%@) is found, while your version is %@.", latest_version, tweak_version];
						}
						UIAlertController *update_alert=[UIAlertController alertControllerWithTitle:(char)[update_str characterAtIndex:0]=='A'?@"Update found":@"Well done" message:update_str preferredStyle:UIAlertControllerStyleAlert];
						[update_alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
						[self presentViewController:update_alert animated:1 completion:nil];
						check_update_in_progress=0;
						[tv reloadData];
					});
				}];
				[task resume];
			});
			return;
		}else if(indexPath.row==3) {
			[tv deselectRowAtIndexPath:indexPath animated:YES];
			[[UIApplication sharedApplication] openURL:[NSURL URLWithString:NSSTR("https://github.com/mapeles/PodsGrant-18")] options:@{} completionHandler:nil];
		}else if(indexPath.row==4) {
			PGSCreditsViewController *creditsVC=[[PGSCreditsViewController alloc] init];
			[self.navigationController pushViewController:creditsVC animated:1];
			[tv deselectRowAtIndexPath:indexPath animated:YES];
		}
		return;
	}
	if(!indexPath.row) {
		PGSProductIDMappingController *newController=[[PGSProductIDMappingController alloc] initWithConfiguration:&_configuration];
		[self.navigationController pushViewController:newController animated:1];
		[tv deselectRowAtIndexPath:indexPath animated:YES];
		return;
	}
}

- (UITableViewCell *)tableView:(id)tv cellForRowAtIndexPath:(NSIndexPath *)indexPath {
	if(indexPath.section==0) {
		UITableViewCell *switchCell=[UITableViewCell new];
		switchCell.textLabel.text=NSSTR("Enabled");
		UISwitch *switchItem=[UISwitch new];
		switchItem.on=_configuration.is_tweak_enabled;
		[switchItem addTarget:self action:@selector(_sw_enabled_switch:) forControlEvents:UIControlEventValueChanged];
		switchCell.accessoryView=switchItem;
		switchCell.selectionStyle=UITableViewCellSelectionStyleNone;
		return switchCell;
	}else if(indexPath.section==1) {
		if(!indexPath.row) {
			UITableViewCell *custom_productid_map_btn=[UITableViewCell new];
			custom_productid_map_btn.textLabel.text=NSSTR("Custom Product ID mapping");
			custom_productid_map_btn.accessoryType=UITableViewCellAccessoryDisclosureIndicator;
			return custom_productid_map_btn;
		}
	}else if(indexPath.section==2){
		if(indexPath.row==3) {
			UITableViewCell *source_code_btn=[UITableViewCell new];
			source_code_btn.textLabel.text=NSSTR("Source Code");
			source_code_btn.textLabel.textColor=[UIColor colorWithRed:0 green:0.478 blue:1 alpha:1];
			return source_code_btn;
		}else if(indexPath.row==2) {
			UITableViewCell *check_update_btn=[UITableViewCell new];
			if(check_update_in_progress) {
				check_update_btn.textLabel.text=@"Checking for update...";
				check_update_btn.textLabel.textColor=[UIColor systemGrayColor];
				return check_update_btn;
			}
			check_update_btn.textLabel.text=NSSTR("Check for Update");
			check_update_btn.textLabel.textColor=[UIColor colorWithRed:0 green:0.478 blue:1 alpha:1];
			return check_update_btn;
		}else if(indexPath.row==4) {
			UITableViewCell *credits_cell=[UITableViewCell new];
			credits_cell.textLabel.text=@"Credits";
			credits_cell.accessoryType=UITableViewCellAccessoryDisclosureIndicator;
			return credits_cell;
		}
		UITableViewCell *actionCell=[UITableViewCell new];
		actionCell.textLabel.text=indexPath.row==1 ? @"Restart SpringBoard" : @"Kill Daemons (Apply Settings)";
		actionCell.textLabel.textColor=[UIColor colorWithRed:0 green:0.478 blue:1 alpha:1];
		return actionCell;
	}
	return nil;
}

- (void)dealloc {
	PGS_freeSettings(&_configuration);
}

@end
