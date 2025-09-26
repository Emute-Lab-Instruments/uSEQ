const findIndex = require('lodash/findIndex');

var Two = function(){
	return findIndex([1, 2, 42], item => item === 42);
};

module.exports = Two;