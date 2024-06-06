var sheetID = 'Replace with Spreadsheet ID';       //spreadsheet ID

function doGet(e) {
  Logger.log(JSON.stringify(e));
  var result = '{';
  if(!e)
    return;
  else
  {
    var sheet = SpreadsheetApp.openById(sheetID).getActiveSheet();
    for (var param in e.parameter)
    {
      Logger.log('In for loop, param=' + param);
      var value = stripQuotes(e.parameter[param]);
      Logger.log(param + ':' + e.parameter[param]);
      // Comparing key sent from board to ensure security and what to send back
      if (param == 'key')
      {
        if (JSON.stringify(value) === JSON.stringify(sheet.getRange('A15').getValue()))
        {
          // Append all the data to the return value in the form of {data,data,data,data}
          result += sheet.getRange('A2').getValue();
          result += ',';
          result += sheet.getRange('B2').getValue();
          result += ',';
          result += sheet.getRange('C2').getValue();
          result += ',';
          result += sheet.getRange('D2').getValue();
          result += '}';

          return ContentService.createTextOutput(result);
        }
        if (JSON.stringify(value) === JSON.stringify(sheet.getRange('A16').getValue()))
        {
          result += sheet.getRange('E2').getValue();
          result += ',';
          result += sheet.getRange('F2').getValue();
          result += '}';

          return ContentService.createTextOutput(result);
        }
      }
    }
    return ContentService.createTextOutput('Data retrieval unavailable for this session');
  }
}

function stripQuotes( value ) {
  return value.replace(/^["']|['"]$/g, "");
}
