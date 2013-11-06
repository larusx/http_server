function PhoneListCtrl($scope,$http){
	$http.get('phones/phones.json').success(function(data){
		$scope.phones=data;
	});
}
