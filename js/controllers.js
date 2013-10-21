/**
 * Created by larus on 13-10-21.
 */
function PhoneListCtrl($scope,$http){
    $http.get('phones/phones.json').success(function(data){
        $scope.phones = data;
    });
    $scope.hello="world";
    $scope.orderProp='age';
}
function PhoneDetailCtrl($scope, $routeParams){
    $scope.phoneId = $routeParams.phoneId;
}